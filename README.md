# Signal generator 3
- Independent signal generator
- Signal generation with modified AD9833 card with amplifier
- User interface: OLED display, 4 buttons (`Left` / `Right`, `Up` / `Down`) and USB serial command line
- Inspired by this project: https://www.instructables.com/id/Signal-Generator-AD9833/

## User interface
- After power-on the device displays `0000000` with the cursor below the MSB and the level set to 0 dBm.
Sine wave is selected, but no signal is output.
- Move the cursor with the `Left` / `Right` buttons and change the selected item with the `Up` / `Down` buttons.
- The frequency is changed digit by digit.
- The up/down arrow switches the display and output frequency between `Freq1` and `Freq2`.
- The amplitude can be changed in 16 steps from about -36 ... + 13 dBm (with the output terminated with 50 Ω).
- To change the amplitude display from dBm (0 dBm corresponds to 1 mW at 50 Ω) to dBu (0 dBu corresponds to 1 mW at 600 Ω)
or dBV (0 dBV corresponds to 1 Vrms), press the `Down` key until the amplitude bar is minimised and the displayed amplitude is -60 dB.
Each further press of the `Down` key then switches between the three possible units.
- The signal shape can be changed between `Sine`, `Triangle` and `Rectangle` and `Off`.
- Output mode is `Constant`, `Sweep 1 s`, `Sweep 3 s`, `Sweep 10 s`, `Sweep 30 s`.
- In the sweep modes `Freq1` and `Freq2` are displayed in two lines.
- The sweep changes the output frequency logarithmically between `Freq1` and `Freq2`, jumps back and starts again.
- After power-on the device displays `0000000 Hz` with the cursor below the MSB and the level set to 0 dBm.
No signal is output.
- By pressing one of the four buttons during power-on it is possible to select 1 kHz (`Up`), 10 kHz (`Down`), 100 kHz (`Right`), 1 MHz (`Left`).

## USB Serial Interface
Communication speed via serial USB (`/dev/ttyUSB0` under Linux, `/dev/tty*` under MacOS, `COMx` under Windows) is 9600 bit/s.
Commands are terminated by a newline, e.g. `"25000S\n"` selects a sine  frequency of 25 kHz.

### Parameter Format
```
[:num:]?[:cmd:]
num = [-]?[0-9]{1,7}[kM]? e.g. '123' or '-10' or '150k' or '1M'
cmd:
?: show status
A: digital pot linear setting, num = 0..256
B: digital pot log setting, num = 0..16
C: -
D: set dB gain, num = -40..+7 (dBV), smaller values = off
E: echo on/off
F: constant freq1
G: sweep 1s from freq1 to freq2
H: sweep 3s from freq1 to freq2
I: sweep 10s from freq1 to freq2
J: sweep 30s from freq1 to freq2
K: n/a (kilo)
L: -
M: n/a (Mega)
N: -
O: output off
P: -
Q: -
R: output rectangle
S: output sine
T: output triangle
U: select dBu
V: select dBV
W: select dBm
X: exchange freq1 and freq2
Y: -
Z: -
```
