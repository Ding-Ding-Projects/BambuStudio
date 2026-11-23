# Cold pull every H2C rack hotend

Open **Device > Hotends & Rack > Cold pull all rack nozzles** with the H2C connected and idle. In the web-based device panel, open the **Hotend** tab and choose the same action. The guide takes an inventory of identified hotends and services them individually. If a rack hotend is already on the right toolhead, the guide starts there. The fixed left hotend is outside this sequence. You can also open the guide from **Local tools > Cold pull all rack hotends** in the title bar.

The cold pull is only ever started by hand, as its own maintenance cycle. It is never added to a print, a print start or a print end, and the guide refuses to begin while the printer is printing, preparing a print, calibrating or updating.

1. With hands and tools clear, confirm **Take nozzle**. The guide waits for the correct hotend on the right toolhead.
2. On the H2C touchscreen, open **Toolbox > Nozzle Cold Pull Maintenance**. Select the right hotend and its material. Follow the firmware prompts for disconnecting the PTFE tube and inserting filament by hand. The manufacturer specifies the previously used filament. Only use a dedicated cleaning filament if the manufacturer supports it for the selected procedure.
3. Follow the touchscreen during heating, purging and cooling. Remove the filament at its prompt, refit the PTFE tube, and check the result. Studio enables result confirmation only after observing the cold-pull status and its subsequent return to idle. A cancelled or unsuccessful procedure must not be confirmed.
4. Confirm successful service, clear the chamber, and select **Return nozzle**. The guide verifies the original serial in the original slot before offering the next hotend.

Close the guide at any time to stop the queue. Closing it cannot stop an operation already accepted by the printer. Use the touchscreen to inspect or stop that operation. A lost connection, stale inventory, changed identity, conflicting activity or timeout stops the guide without another movement command. It requires current occupancy and hotend serial reports. Firmware that omits these fields cannot run this sequence. No service resumes automatically after reconnecting.

The native rack exchange protocol handles taking and returning tools. Studio has a cold-pull activity report but no documented command to start the maintenance cycle remotely. The operator confirms the touchscreen result because the activity report does not distinguish successful completion from cancellation.

[H2C 3D Printing User Manual, section 10.5.3](https://csm.bblcdn.com/hub/eff78da43720461787dc8bbe5fa0372d.pdf#page=151)
