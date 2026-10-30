bpad Audio Conversion Tool
===========================

Option 1 (Easiest - In-Browser, No Dependencies):
  1. Double-click `audio_converter.html` in your web browser (Chrome, Edge, Firefox).
  2. Drag & drop any audio file (MP3, WAV, AAC, OGG).
  3. Listen to the preview player to verify the 4kHz sound quality.
  4. Click "Download test_audio.h" and save it in this sketch folder.

Option 2 (Command Line via ffmpeg + Python):
  1. Convert to 4000 Hz mono WAV with anti-aliasing filter:
     ffmpeg -i input.mp3 -ar 4000 -ac 1 -acodec pcm_s16le -af "lowpass=f=1800" test.wav
  2. Generate test_audio.h:
     python -c "
     d = open('test.wav','rb').read()
     with open('test_audio.h','w') as f:
         f.write('#pragma once\n#include <Arduino.h>\nconst uint8_t audio_data[] PROGMEM = {\n')
         for i in range(0, len(d), 16):
             f.write('  ' + ', '.join(hex(b) for b in d[i:i+16]) + ',\n')
         f.write('};\nconst uint32_t audio_data_len = ' + str(len(d)) + ';\n')
     print(f'Done: {len(d)} bytes')
     "
