from pydub import AudioSegment
import scipy.io.wavfile
import numpy as np
import base64
import io

if __name__=="__main__":
    infile = "/Users/alex/Desktop/float-test/stop-2"
    outfile = "/Users/alex/Desktop/float-test/stop.wav"

    f = open(infile, "r")

    byte_data = bytearray()
    vals = f.read(1000)
    # Reading all at once didn't work for some reason
    while vals:
        byte_data.extend(base64.b64decode(vals))
        vals = f.read(1000)
        if len(vals) != 1000: break
    f.close()

    # Enforce little endian
    dt = np.dtype(np.float32)
    dt = dt.newbyteorder('<')

    # It appears that a few bytes are lost during the serial data
    # transmission. This causes all the float data to be messed up
    # after a byte loss, so if we detect strange float values then
    # skip those THREE bytes (because one is lost)
    data = np.zeros(int(len(byte_data)/4), dtype=dt)
    num_idx = 0
    byte_idx = 0
    while byte_idx < len(byte_data)-4:
        num = np.frombuffer(byte_data[byte_idx:byte_idx+4], dtype=dt)
        # Data coming off pico should be normalized between -1 and 1.
        # If it's not, we know that there was some data loss.
        if (num > 1 or num < -1):
            #print("Skipped one")
            byte_idx = byte_idx + 3
        else:
            data[num_idx] = num[0]
            num_idx = num_idx+1
            byte_idx = byte_idx + 4
    
    wav_io = io.BytesIO()
    scipy.io.wavfile.write(wav_io, 4000, data)
    wav_io.seek(0)
    sound = AudioSegment.from_wav(wav_io)
        
    sound.export(outfile, format="wav")
