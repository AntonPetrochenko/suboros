SchizoOS - OS for the Subor Famiclone, because we have time to burn and money to waste on Claude tokens.
========

```
Note: LLMs WERE used in creation of this project, because heck no we aren't writing a fully fledged OS for a shitass famiclone manually. 
We ARE doing the actual OS architecture ourselves though, but this thing speeds it up menial tasks. And burns tokens like crazy.

Just an extra layer of "what are you doing with your life" this project desperately needs
```

## Rant
Subor SB255-B is an "iconic" shitass famiclone. It has a decent keyboard built in, so it's kind of a Family BASIC device. It often shipped with Family BASIC with save features gutted out.

Funnily enough, it's the exact famiclone I've had as a kid, and messing around with BASIC on it is the exact thing that eventually led to me becoming a software engineer.

I want to bring new life to it, in the most scuffed way possible. When I saw a freaking LPT port on it, I figured how it could interface with it. A schizo internal rant ensued where I've imagined an actual OS running on it, and figured "why the heck not?"

## Wtf?
My target for development is Everdrive N8 and emulator support, "shitass peripheral interface", real file system and device drivers. Maybe multitasking

## Why?????
Because lol.

## Target hardware
- Subor SB225-B
- Everdrive N8 (MMC1)
- Shitass Peripheral Interface

## Features, in somewhat particular order

- [x] Feel out the hardware available - learn to use MMC1 and it's large amount of PRG RAM, which we'll use to reach Commodore 64 levels of RAM, just slower
- [ ] Make driver model - abstract away hardware, so we can ~~eventually upgrade to Subor SB2000 with floppy drive~~
- [ ] Keyboard driver - support both real Family BASIC and Subor
- [ ] Proper PPU driver - abstract away the PPU because lmao, make text mode and graphics mode
- [x] Real file system
- [x] Syscalls to actually do stuff
- [ ] Shitass Peripheral Interface

## Shitass Peripheral Interface (ShPI)

Shitass Peripheral Interface is going to work by combining LPT port and one of the controller inputs. LPT for outgoing data, controller port for input data. That way we can both read and write data to external devices!!!
