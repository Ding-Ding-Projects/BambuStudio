import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Source contracts supplement, and do not replace, the native Windows race run.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = name => fs.readFileSync(path.join(root, 'src/slic3r/GUI', name), 'utf8').replaceAll('\r\n', '\n');
const source = {
    worker: read('BackgroundSlicingProcess.cpp'), plater: read('Plater.cpp'),
    frame: read('MainFrame.cpp'), plate: read('PartPlate.cpp')
};
const body = (text, name) => {
    const start = text.indexOf(name);
    assert.notEqual(start, -1, name + ' exists');
    const end = text.indexOf('\n}', start);
    assert.notEqual(end, -1, name + ' has a body');
    return text.slice(start, end);
};
const contracts = [
    {
        name: 'old completion rejected before worker stop or pending consumption',
        check: s => {
            const fn = body(s.plater, 'void Plater::priv::on_process_completed(');
            const identity = fn.indexOf('is_current_slice_event(evt.generation(),');
            return identity >= 0 && identity < fn.indexOf('this->background_process.stop();') &&
                identity < fn.indexOf('m_pending_slice_output.consume(');
        },
        mutate: s => ({...s, plater: s.plater.replace('is_current_slice_event(evt.generation(),', 'ignored_identity(evt.generation(),')})
    },
    {
        name: 'worker completion carries current native generation',
        check: s => body(s.worker, 'void BackgroundSlicingProcess::thread_proc()').includes('exception, automation_generation());'),
        mutate: s => ({...s, worker: s.worker.replace('exception, automation_generation());', 'exception);')})
    },
    {
        name: 'blocking ownership barriers never orphan a live worker',
        check: s => !s.worker.includes('m_orphaned_threads') && !s.worker.includes('.detach()') &&
            ['bool BackgroundSlicingProcess::stop()', 'void BackgroundSlicingProcess::stop_internal()'].every(name =>
                body(s.worker, name).includes('m_condition.wait(lck, [this](){ return m_state == STATE_CANCELED; });')),
        mutate: s => ({...s, worker: s.worker.replace('m_condition.wait(lck, [this](){ return m_state == STATE_CANCELED; });', 'm_thread.detach();')})
    },
    {
        name: 'user cancellation requests cancellation without waiting or claiming completion',
        check: s => {
            const fn = body(s.worker, 'bool BackgroundSlicingProcess::request_stop()');
            return fn.includes('m_print->cancel();') && !fn.includes('m_condition.wait') &&
                !fn.includes('m_state = STATE_IDLE') && !fn.includes('m_automation_outcome = 4');
        },
        mutate: s => ({...s, worker: s.worker.replace('m_cancel_requested = true;', 'm_cancel_requested = true; m_state = STATE_IDLE;')})
    },
    {
        name: 'document replacement stops worker before deleting plate ownership',
        check: s => {
            const fn = body(s.plater, 'bool Plater::priv::reset(');
            return fn.indexOf('background_process.reset();') >= 0 &&
                fn.indexOf('background_process.reset();') < fn.indexOf('partplate_list.reinit();');
        },
        mutate: s => ({...s, plater: s.plater.replace('    background_process.reset();\n    Plater::TakeSnapshot', '    Plater::TakeSnapshot')})
    },
    {
        name: 'explicit action starts before preview can trigger automatic slicing',
        check: s => {
            const begin = s.frame.indexOf('auto start_slice =');
            const fn = s.frame.slice(begin, s.frame.indexOf('m_slice_btn->Bind(wxEVT_BUTTON', begin));
            return fn.indexOf('ProcessEvent(slice_event);') >= 0 &&
                !fn.includes('SetSelection(tpPreview)');
        },
        mutate: s => ({...s, frame: s.frame.replace('ProcessEvent(slice_event);', 'QueueEvent(slice_event.Clone());')})
    },
    {
        name: 'native progress carries its origin and is checked before display',
        check: s => s.plate.includes('status, process.automation_generation(), this)') &&
            body(s.plater, 'void Plater::priv::on_slicing_update(').includes('evt.plate != partplate_list.get_curr_plate()'),
        mutate: s => ({...s, plate: s.plate.replace('status, process.automation_generation(), this)', 'status)')})
    },
    {
        name: 'display refresh is not a cancellation request',
        check: s => !body(s.frame, 'void MainFrame::update_slice_print_status(').includes('cancel_pending_print_after_slice'),
        mutate: s => ({...s, frame: s.frame.replace('// Status refreshes are observations, never cancellation requests.', 'm_plater->cancel_pending_print_after_slice();')})
    }
];
for (const contract of contracts) {
    assert.equal(contract.check(source), true, contract.name);
    assert.equal(contract.check(contract.mutate(source)), false, contract.name + ' negative mutation');
    console.log('PASS: ' + contract.name + ' (including negative mutation)');
}
console.log(contracts.length + ' source contracts passed; native runtime verification remains separate.');
