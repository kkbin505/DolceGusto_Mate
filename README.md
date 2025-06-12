# DolceGusto_Mate
A control mate for DolceGusto coffee machine

![Uploading 2025-05-07_25a6f992bdf618.jpg…]()


# History
I have a DolceGusto machine, how ever the selection of capsules was limited, so I decided to convert my capsule coffee machine into an espresso machine. That’s how this project started — I disassembled a Nespresso machine, kept most of its original parts, and added a 58mm brew group along with a ESP32 control module.

# Software

For the software, I used an ESP32 combined with a MOSFET to control the coffee machine’s power, allowing for timed startup and automatic shutdown.
This is achieved by controlling the extraction switch on the coffee machine’s original mainboard. Normally, the extraction switch is at a high level, and pulling it to ground starts the extraction; releasing it (returning to high) stops it. I replaced the original mechanical switch with a solid-state switch controlled by the ESP32. The MOSFET I used is an NPN-type MOS, salvaged from an FPV fligh controller.

# Hardware

On the hardware side, I added silicone tubing, a 4-way solenoid valve, a pressure gauge, and PTFE tubing, all connected to a 58mm brew group from Paicent.The pressure is regulated to 9 bar using a mechanical pressure relief valve.
I also designed 3D-printed parts to mount the additional components, and replaced the original conductive silicone switch with a mechanical keyboard switch, as it provides a much better tactile feel.





