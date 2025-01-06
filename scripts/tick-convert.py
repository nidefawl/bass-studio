TICK_BITS=12
TICK_BITS_BAR=(TICK_BITS + 2)
TICKS_QUARTER=(1 << TICK_BITS)
TICKS_16TH=(TICKS_QUARTER >> 2)
TICKS_BAR=(TICKS_QUARTER << 2)
TICK_MASK_16TH=(TICKS_16TH - 1)
TICK_MASK_SUB_16TH=((TICKS_16TH >> 1) - 1)

TPQ_OVER_MINUTE_100 = TICKS_QUARTER/6000.;
MINUTE_100_OVER_TPQ = 6000.0/TICKS_QUARTER;


def secondsToTickDD(bpm):
    return TPQ_OVER_MINUTE_100 * bpm
def secondsToTickDD2(bpm):
    return (TICKS_QUARTER/6000.) * bpm
def secondsToTickDD3(bpm):
    return (bpm/6000.) * TICKS_QUARTER


for bpm in range(60, 200, 10):
    results = [secondsToTickDD(bpm), secondsToTickDD2(bpm), secondsToTickDD3(bpm)]
    print(results)
