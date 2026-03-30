import numpy as np
# import matplotlib
# matplotlib.use("TkAgg")   # oder "QtAgg", falls Qt installiert ist
import matplotlib.pyplot as plt
from scipy.signal import freqz

# =========================
# PARAMETER
# =========================
fs = 44100            # Samplerate
N  = 8192             # FFT-Auflösung

# Liste von Biquads (Reihenfolge = Signalfluss)
biquads = [
([1.026721, -1.919612, 0.901093], [1.0, -1.922258, 0.925168]), # LOWSHELF
([0.802626,  -1.424665,  0.670208 ], [1.0, -1.424665,  0.472834 ]), # PEAKINGEQ
([2.092489, -2.262849, 0.808592], [1.0, -0.597000, 0.235232]), # HIGHSHELF
]

w = np.linspace(0, np.pi, N)
H = np.ones_like(w, dtype=complex)

for b, a in biquads:
    _, h = freqz(b, a, worN=w)
    H *= h

f = w * fs / (2*np.pi)
mag_db = 20 * np.log10(np.abs(H) + 1e-12)

# =========================
# PLOT
# =========================
plt.figure(figsize=(9,5))
plt.semilogx(f, mag_db)
plt.xlim(20, fs/2)
plt.ylim(-15, 15)
plt.grid(True, which='both')
plt.xlabel("Frequency (Hz)")
plt.ylabel("Amplitude (dB)")
plt.title("Biquad frequency response")
plt.tight_layout()
# plt.show()
plt.savefig("biquad_response.png", dpi=150)
print("Plot gespeichert als biquad_response.png")