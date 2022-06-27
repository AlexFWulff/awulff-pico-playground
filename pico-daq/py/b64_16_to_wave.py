from pydub import AudioSegment
import numpy as np
import base64

if __name__=="__main__":
    infile = "/Users/alex/Desktop/pico/other-3"
    outfile = "/Users/alex/Desktop/pico/other.wav"

    f = open(infile, "r")

    byte_data = bytearray()
    vals = f.read(6000)
    # Reading all at once didn't work for some reason
    while vals:
        byte_data.extend(base64.b64decode(vals))
        vals = f.read(6000)
    f.close()

    # Enforce little endian
    dt = np.dtype(np.uint16)
    dt = dt.newbyteorder('<')
    data = np.frombuffer(byte_data, dtype=dt)
    
    data = data.astype(np.int32)
    data = data-np.mean(data)
    data = data.astype(np.int16)
    
    audio = AudioSegment(
        data.tobytes(),
        sample_width=2,
        frame_rate=5000,
        channels=1
    )
    
    audio.export(outfile, format="wav")
