# Cool down after a print

When a print ends, Bambu Studio can turn every fan on the printer to full speed and keep them
running until the printer is cool enough to open, for five minutes at most. It is off until you
turn it on.

## The Cool down card

The card sits on the Device page under the **Door chime** card.

- **Cool down the printer after a print** turns the option on or off.
- The line under the switch says what is happening now: off, no printer connected, ready, cooling
  (with the bed and chamber temperatures and the time left before the limit), cooled down, or
  stopped at the five minute limit.

The switch is kept in the application settings (`print_cooldown_enabled`) and applies to
whichever printer is selected on the Device page.

## What happens when a print ends

1. The print status of the selected printer changes from printing to finished, failed or stopped.
2. If the bed is above 40 °C, or the chamber is above 40 °C on a printer that reports its chamber
   temperature, Bambu Studio sends `M106 P1 S255`, `M106 P2 S255` and `M106 P3 S255`: the part
   cooling fan, the auxiliary fan and the chamber fan at full speed. A printer without one of
   these fans ignores that line.
3. Once the bed and the chamber are both at or under 40 °C, the fans are turned off with
   `M106 P1 S0`, `M106 P2 S0` and `M106 P3 S0`.
4. If that has not happened five minutes after the fans went on, they are turned off anyway and
   the card says the printer may still be warm.

If the printer is already cool when the print ends, no fan is turned on.

## Limits and failure modes

- Bambu Studio sends the commands itself, so it must stay open with the printer selected and
  connected from the end of the print until the fans go off. The option does not change the
  sliced file, and a print sent from another device is cooled only if this copy of Bambu Studio
  is watching the printer when it ends.
- Only a print that this session saw running starts a cool down. Opening Bambu Studio beside a
  printer that finished earlier does nothing.
- If Bambu Studio closes, loses the printer, or you select another printer while the fans are
  running, it cannot turn them off. Turn them off on the printer, or start the next print, which
  sets the fans itself.
- A new print that starts during the cool down takes over the fans; Bambu Studio sends nothing
  more for the old one.
- Turning the switch off during a cool down turns the fans off straight away.
- The nozzle temperature is not checked. The limit of 40 °C and the five minutes are fixed.
- When fan control is locked for the printer, the fans are left alone and the card says so.

## Security

The commands go to the printer over the connection it already has with Bambu Studio. Nothing is
sent anywhere else and no new setting is stored on the printer.

## Verification

`tests/print_cooldown.test.mjs` compiles and runs `tests/print_cooldown_test.cpp`, which checks
the status and temperature rules, the fan G-code, a normal cool down, the five minute limit, an
already cool printer, the switch being off or turned off part way, a new print taking over, and
switching printers. The card was captured from the built application. The cool down has not been
run on a real printer.
