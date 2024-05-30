This branch contains experimental changes to `sfizz_jack` to also accept MIDI input from an ALSA port.
This is done mainly so that I can use Sfizz with Rosegarden.

Although Rosegarden is a JACK-based DAW, all MIDI is done through ALSA, not JACK.
So to use Sfizz with Rosegarden we have two choices:
1. Teach Sfizz how to accept ALSA-style MIDI input, or
2. Write a bridge that forwards Rosegarden’s ALSA-style MIDI output to Sfizz through JACK.

For some reason, I chose to try (1) first even though (2) is probably easier.
