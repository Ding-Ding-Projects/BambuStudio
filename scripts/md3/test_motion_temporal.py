"""Product-free contracts for the actual temporal acceptance and task lifetime.

Run on the hosted Python route. These tests do not prove capture cadence, pixels,
MCP process teardown, composition, or a rendered product transition.
"""
import ast
import asyncio
import copy
import inspect
import unittest

import motion_temporal as production


def point(ms):
    return {"monotonic_ns": ms * 1_000_000,
            "utc": "2026-10-03T00:00:00.000000+00:00"}


def interval(start, end):
    return {"start": point(start), "end": point(end)}


def observations():
    return interval(10, 15), [
        {"interval": interval(0, 5), "roi_sha256": "a" * 64},
        {"interval": interval(35, 45), "roi_sha256": "b" * 64},
        {"interval": interval(70, 80), "roi_sha256": "c" * 64},
        {"interval": interval(190, 205), "roi_sha256": "d" * 64},
    ]


def binding():
    return {"schema": 1, "run_id": "123", "run_attempt": "1",
            "source_commit": "a" * 40, "verifier_commit": "b" * 40,
            "exe_sha256": "c" * 64, "package_sha256": "d" * 64,
            "pid": 42, "process_started": 123.5, "hwnd": 456,
            "desktop": "owned-temporal", "job_name": "Local\\BambuNativeScale-" + "e" * 64,
            "language": "en", "theme": "light", "dpi": 96,
            "client_size": [1200, 800], "roi": [10, 10, 30, 30],
            "click": {"x": 20, "y": 20, "button": "left", "target_hwnd": 789},
            "nonce": "f" * 32}


class TemporalContract(unittest.TestCase):
    def test_two_whole_intermediate_intervals(self):
        self.assertEqual(production.intermediate_indices(*observations()), [1, 2])

    def test_one_qualified_interval_is_enough(self):
        click, frames = observations()
        frames[2]["interval"] = interval(120, 130)
        self.assertEqual(production.intermediate_indices(click, frames), [1])

    def test_late_frames_are_not_observed(self):
        click, frames = observations()
        frames[1]["interval"], frames[2]["interval"] = interval(111, 120), interval(130, 140)
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_interval_straddling_deadline_is_not_intermediate(self):
        click, frames = observations()
        frames[1]["interval"], frames[2]["interval"] = interval(105, 115), interval(120, 130)
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_acknowledgement_must_precede_entire_interval(self):
        click, frames = observations()
        click["end"] = point(75)
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_pixel_change_must_differ_from_both_endpoints(self):
        click, frames = observations()
        frames[1]["roi_sha256"], frames[2]["roi_sha256"] = "a" * 64, "d" * 64
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_return_to_baseline_is_outside_this_contract(self):
        click, frames = observations()
        frames[-1]["roi_sha256"] = frames[0]["roi_sha256"]
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_baseline_cannot_overlap_input(self):
        click, frames = observations()
        frames[0]["interval"] = interval(0, 11)
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_final_frame_cannot_be_early(self):
        click, frames = observations()
        frames[-1]["interval"] = interval(185, 195)
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_four_frame_budget_and_malformed_rows(self):
        click, frames = observations()
        for bad in (frames[:3], frames + frames[:1], [None] + frames[1:]):
            self.assertEqual(production.intermediate_indices(click, bad), [])

    def test_monotonic_order_and_utc_are_required(self):
        click, frames = observations()
        for bad in (interval(80, 70), {"start": point(0), "end": {"monotonic_ns": 2, "utc": "local"}}):
            changed = copy.deepcopy(frames)
            changed[1]["interval"] = bad
            self.assertEqual(production.intermediate_indices(click, changed), [])

    def test_overlapping_frame_intervals_are_rejected(self):
        click, frames = observations()
        frames[1]["interval"] = interval(35, 75)
        self.assertEqual(production.intermediate_indices(click, frames), [])

    def test_strict_owned_binding(self):
        production.validate_binding(binding())
        for key, value in (("schema", True), ("pid", True), ("desktop", "Default"),
                           ("job_name", "unowned"), ("process_started", float("nan")),
                           ("client_size", [8192, 8192]), ("roi", [0, 0, 1201, 10]),
                           ("nonce", []), ("dpi", 100)):
            changed = binding()
            changed[key] = value
            with self.assertRaises(RuntimeError):
                production.validate_binding(changed)

    def test_input_requires_exact_child_and_in_bounds_point(self):
        for change in ({"target_hwnd": True}, {"x": 1200}, {"button": "middle"}):
            changed = binding()
            changed["click"].update(change)
            with self.assertRaises(RuntimeError):
                production.validate_binding(changed)

    def test_start_only_mutation_cannot_pass_straddling_case(self):
        # Compile a deliberate weakening of the actual production predicate:
        # an early request start cannot stand in for a fully bounded observation.
        tree = ast.parse(inspect.getsource(production.intermediate_indices))
        replaced = 0
        for node in ast.walk(tree):
            if isinstance(node, ast.Compare) and any(isinstance(op, ast.Lt) for op in node.ops):
                for child in ast.walk(node.left):
                    if isinstance(child, ast.Constant) and child.value == "end":
                        child.value = "start"
                        replaced += 1
        self.assertEqual(replaced, 1)
        namespace = dict(production.__dict__)
        exec(compile(ast.fix_missing_locations(tree), "temporal-start-only-mutant", "exec"), namespace)
        click, frames = observations()
        frames[1]["interval"], frames[2]["interval"] = interval(105, 115), interval(120, 130)
        self.assertEqual(production.intermediate_indices(click, frames), [])
        self.assertEqual(namespace["intermediate_indices"](click, frames), [1])


class InputLifetimeContract(unittest.IsolatedAsyncioTestCase):
    async def test_normal_request_is_joined(self):
        async def operation():
            await asyncio.sleep(0)
            return 7
        async with production.joined_input(operation()) as task:
            pass
        self.assertTrue(task.done())
        self.assertEqual(task.result(), 7)

    async def test_capture_exception_cancels_and_joins_input(self):
        entered, closed = asyncio.Event(), asyncio.Event()
        async def operation():
            try:
                entered.set()
                await asyncio.Event().wait()
            finally:
                closed.set()
        with self.assertRaisesRegex(RuntimeError, "synthetic capture failure"):
            async with production.joined_input(operation()) as task:
                await entered.wait()
                raise RuntimeError("synthetic capture failure")
        self.assertTrue(task.cancelled())
        self.assertTrue(closed.is_set())

    async def test_parent_cancellation_is_not_swallowed(self):
        entered, closed = asyncio.Event(), asyncio.Event()
        async def operation():
            try:
                entered.set()
                await asyncio.Event().wait()
            finally:
                closed.set()
        async def parent():
            async with production.joined_input(operation()):
                await asyncio.Event().wait()
        task = asyncio.create_task(parent())
        await entered.wait()
        task.cancel()
        with self.assertRaises(asyncio.CancelledError):
            await task
        self.assertTrue(closed.is_set())


if __name__ == "__main__":
    unittest.main()
