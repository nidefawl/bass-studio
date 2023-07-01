'''User defined path functions'''
import math
import random
import array
import functools
from cmath import pi
from path_bindings import *

def smoothstep(x):
    return x * x * (3 - 2 * x)


def randomizePath1(listIn):
    path = None
    if listIn is not None:
        path = []
        strength = 0.1
        for pt in listIn:
            ptOut = pt
            for i in range(-4, 5, 1):
                fScale = i / 4.0 * 0.6
                ptOut.x += (-random.random() + random.random()) * fScale * strength
                ptOut.y += (-random.random() + random.random()) * fScale * strength
                path.append(ptOut)
    return path


frameNum = 0
frameNum2 = 0


def clamp(x, a, b):
    if x > b:
        return b
    if x < a:
        return a
    return x

def scaledLvlCl(lvl, fMin, fMax):
    return fMin + (1.0 - lvl) * (fMax - fMin)

def bpm2Tm(bpm):
    return 60000.0 / bpm


def bpm2TmSec(bpm):
    return 60.0 / bpm


# @functools.lru_cache(maxsize=256)
def tmToBeat(tm, bpm, nQuarters):
    ftimestep = bpm2TmSec(u_bpm)
    fProgr = (tm / ftimestep) / float(nQuarters)
    iBarNum = int(math.floor(fProgr))
    fBarProgr = fProgr - iBarNum
    return (fBarProgr, iBarNum)


u_bpm = 120.0


def randomizePathEye(listIn, t):
    path = []
    strength = 1.1
    # random.seed(frameNum+math.floor(frameNum2/4))
    # frameNum += 1
    pos = random.random()
    # t += frameNum*1000.0
    af = ((-t / 1150.0) % 3.0) / 2.5
    fRange = 0.04
    for i in range(0, 32, 1):
        fScale = (i * i) / 3.0 * 2.6 * af
        minAngle = pos - fRange
        maxAngle = pos + fRange
        posOffset = minAngle + (maxAngle - minAngle) * random.random()
        t2 = clamp((t + i) / 20.0, 0.0, 1.0)
        fScale2 = clamp(strength * fScale * af, 0.2 * t2, 1.0)
        x = math.sin(posOffset * pi * 2.0) * fScale2
        y = math.cos(posOffset * pi * 2.0) * fScale2
        ptOut = Vec(x, y)
        # ptOut.x += (random.random())*fScale*strength
        # ptOut.y += (random.random())*fScale*strength
        path.append(ptOut)
    path = path + path
    path = path + path
    path = path + path
    return path


def trippyKaleidoscope(listIn, t):
    path = []
    strength = 1.1
    random.seed(frameNum + math.floor(frameNum2 / 4))
    # frameNum += 1
    pos = random.random()
    # t += frameNum*1000.0
    t *= 0.1
    af = ((-t / 1150.0) % 3.0) / 2.5
    fRange = 0.24 * ((t * 0.2) % 1.0) + 0.04
    fsin = math.sin(t * 4) + 1.0
    iStep = 3 + math.floor(fsin * 32)
    for i in range(0, iStep, 1):
        fScale = (i * i) / 3.0 * 2.6 * af
        minAngle = pos - fRange
        maxAngle = pos + fRange
        randPos = (t % 100.0) + i * 0.3
        posOffset = minAngle + (maxAngle - minAngle) * randPos
        t2 = clamp((t + i) / 20.0, 0.0, 1.0)
        fScale2 = clamp(strength * fScale * af, 0.2 * t2, 1.0)
        x = math.sin(posOffset * pi * 2.0) * fScale2
        y = math.cos(posOffset * pi * 2.0) * fScale2
        ptOut = Vec(x, y)
        # ptOut.x += (random.random())*fScale*strength
        # ptOut.y += (random.random())*fScale*strength
        path.append(ptOut)
    path = path + path
    path = path + path
    path = path + path
    path = path + path
    path = path + path
    return path


class FastRand:
    def __init__(self, rng_state=1140671485):
        self.rng_state = rng_state

    def rand32Bits(self):
        rndState = (self.rng_state * 6364136223846793005 + 1) & 0xFFFFFFFFFFFFFFFF
        self.rng_state = rndState
        xorshifted = (((rndState >> 18) ^ rndState) >> 27) & 0xFFFFFFFF
        rot = rndState >> 59 & 0x1F
        return ((xorshifted >> rot) | (xorshifted << ((-rot) & 0x1F))) & 0xFFFFFFFF

    def randomNeg1Pos1(self):
        bits = self.rand32Bits()
        return (bits & 0xFFFFFFFF) / (1.0 * 0x7FFFFFFF)

    def random(self):
        bits = self.rand32Bits()
        return (bits & 0xFFFFFFFF) / (1.0 * 0x7FFFFFFF)

    def seed(self, seed):
        self.rng_state = seed


def seededRndFloat(seed):
    rndState = (seed * 6364136223846793005 + 1) & 0xFFFFFFFFFFFFFFFF
    xorshifted = (((rndState >> 18) ^ rndState) >> 27) & 0xFFFFFFFF
    rot = rndState >> 59 & 0x1F
    ret = ((xorshifted >> rot) | (xorshifted << ((-rot) & 0x1F))) & 0xFFFFFFFF
    f = (ret & 0xFFFFFFFF) / (1.0 * 0x7FFFFFFF)
    return f - 1.0


def seededRandom(seed):
    rndState = (seed * 6364136223846793005 + 1) & 0xFFFFFFFFFFFFFFFF
    xorshifted = (((rndState >> 18) ^ rndState) >> 27) & 0xFFFFFFFF
    rot = rndState >> 59 & 0x1F
    ret = ((xorshifted >> rot) | (xorshifted << ((-rot) & 0x1F))) & 0xFFFFFFFF
    f = (ret & 0x7FFFFFFF) / (1.0 * 0x7FFFFFFF)
    return f


def trippyKaleidoscopeBase(listIn, t, tSpiral):
    fProgr, fProgrTick = tmToBeat(t, u_bpm, 4)
    pos1 = seededRandom(fProgrTick)
    pos2 = seededRandom(fProgrTick + 1)
    pos = pos1 + (pos2 - pos1) * fProgr

    rangeStep = 128.0

    path = []
    for i in range(0, int(rangeStep), 1):
        posOffset = pos + i / (rangeStep - 1.0) + 32.0
        radius = math.sin(tSpiral * -4 + i / rangeStep)
        x = math.sin(posOffset * pi * 2.0) * radius
        y = math.cos(posOffset * pi * 2.0) * radius
        ptOut = Vec(x, y)
        path.append(ptOut)
    return [path]


def pathGen_trippyKaleidoscope2(listIn, t):
    return trippyKaleidoscopeBase(listIn, t, t)


def pathGen_trippyKaleidoscope5(listIn, t):
    fProgrQ, fProgrTickQ = tmToBeat(t, u_bpm, 8.0)
    return trippyKaleidoscopeBase(listIn, t, fProgrQ * 4.0 + fProgrTickQ)


def pathGen_testProcFFT(fftBandsIn, t):
    cx = 0
    cy = -0.2
    xScale = 1.0
    path = []
    for side, fftBands in enumerate(fftBandsIn):
        rangeStep = len(fftBands)
        channelVecs = []
        radiusOffset = (
            0.7
            * scaledLvlCl(fftBands[2], 0.1, 1.1)
            * scaledLvlCl(fftBands[3], 0.1, 1.1)
            * scaledLvlCl(fftBands[4], 0.1, 1.1)
        )
        for i, bandVal in enumerate(fftBands):
            bandValLog = scaledLvl(bandVal)
            posOffset = i / (rangeStep - 1.0)
            radius = (1.0 - radiusOffset) * bandValLog + radiusOffset
            x = cx + math.sin(posOffset * pi * 1) * radius * xScale * (side * 2 - 1)
            y = cy - math.cos(posOffset * pi * 1) * radius
            channelVecs.append(Vec(x, y))

    path.append(channelVecs)
    last1 = path[1][-1]
    path[1].append(path[0][-1])
    path[0].append(last1)
    return path


def pathGen_testBPM(listIn, t):
    fProgr, fProgrTick = tmToBeat(t, u_bpm, 1)
    path = []
    aOrB = fProgrTick % 2
    nSteps = 16
    for i in range(nSteps):
        x = fProgr * 2.0 - 1.0
        y = (i / (nSteps - 1.0)) * 2.0 - 1.0
        if aOrB == 0 and i % 2 == 1:
            x -= 0.1
        path.append(Vec(x, y))
    return [path]


def pathGen_triangleExpanding(listIn, t):
    fProgr, fProgrTick = tmToBeat(t, u_bpm, 4)
    fProgrQ, fProgrTickQ = tmToBeat(t, u_bpm, 2)
    pos1 = seededRndFloat(fProgrTick)
    pos2 = seededRndFloat(fProgrTick + 1)
    pos = pos1 + (pos2 - pos1) * fProgr

    rangeStep = 4
    cx = seededRndFloat(7 * math.floor(fProgrTickQ)) * 0.9
    cy = seededRndFloat(7 * math.floor(fProgrTickQ) + 1) * 0.5

    path = []
    radius = fProgrQ * 1.6
    radius *= radius

    for i in range(0, int(rangeStep), 1):
        # t+=i*0.12
        posOffset = pos + i / (rangeStep - 1.0)
        # radius = 0.75+0.25*math.sin(t*-4+i/rangeStep*0.25)
        x = cx + math.sin(posOffset * pi * 2.0) * radius
        y = cy - math.cos(posOffset * pi * 2.0) * radius
        # list.append(Vec(x, y))
        rr = (((t + i) * 2.12) % 1.0) * 0.4 + 0.1
        rr *= 2.2
        for j in range(0, 4, 1):
            x += seededRndFloat(fProgrTick + i + 1234123) * rr
            y += seededRndFloat(fProgrTick + i + 423525) * rr
            path.append(Vec(x, y))
            rr *= rr
        radius *= radius
    return [path]


def genShapes(fftBandsIn, t, fftOffset, fftBins, rangeStep = 32):
    # t = 457.
    # t *=0.1
    # b = scaledLvlCl(fftBandsIn[0][8]-0.002, 0.0, 1.0)
    # if b > 0.6:
    #   # return [[],[]]
    #   t *= 3.0

    fProgr, fProgrTick = tmToBeat(t, u_bpm, 4)
    fProgrQ, fProgrTickQ = tmToBeat(t, u_bpm, 2)
    pos1 = seededRndFloat(fProgrTick)
    pos2 = seededRndFloat(fProgrTick + 1)
    posBegin = pos1 + (pos2 - pos1) * fProgr

    cx = seededRndFloat(7 * math.floor(fProgrTickQ)) * 1.6
    cy = seededRndFloat(7 * math.floor(fProgrTickQ) + 1) * 0.8
    cx1 = seededRndFloat(fProgrTick) * 1.6
    cy1 = seededRndFloat(fProgrTick + 214124)
    cx2 = seededRndFloat(fProgrTick + 1) * 1.6
    cy2 = seededRndFloat(fProgrTick + 214124 + 1)
    cx = cx1 + (cx2 - cx1) * fProgr
    cy = cy1 + (cy2 - cy1) * fProgr

    paths = []
    for j in range(2):
        channelPath = []
        vec = None
        radius = (1.0 - abs(fProgrQ * 2.0 - 1.0)) * 1.6
        # radius = fProgrQ*0.1+0.2
        minOneOne = fProgrQ * 2.0 - 1.0
        radius = (1.0 - abs(minOneOne)) * 0.02 + 0.02
        # radius = math.pow(radius, 4)
        fftBands = fftBandsIn[j]
        if fftOffset + fftBins >= len(fftBands):
            fftBands = fftBands[-fftBins:]
        else:
            fftBands = fftBands[fftOffset: (fftOffset + fftBins)]
            # print(fftOffset, fftOffset+fftBins, len(fftBands))

        numBands = len(fftBands)
        fftStepSize = numBands / rangeStep
        pos = (posBegin + j * 0.5 + (t * 2.5 * ((fProgrTick % 2) * 2 - 1))) % 1.0
        for i in range(0, rangeStep):
            # t+=i*0.12
            posOffset = pos + i / (rangeStep - 1.0)
            # radius = 0.75+0.25*math.sin(t*-4+i/rangeStep*0.25)
            scale = 0.0
            bandIdx = numBands - 1 - round(i * fftStepSize)
            if i < rangeStep - 1 and bandIdx >= 0 and bandIdx < numBands:
                scale = scaledLvlCl(fftBands[bandIdx], 0.0, i * 0.11)
                scale += scaledLvlCl(
                    fftBands[bandIdx], 0.0, 128.0 / (bandIdx * bandIdx + 9.0)
                )
                scale *= 0.125
                radius += scale * 0.01
            ff = 1.0
            x = cx + math.sin(posOffset * pi * 2.0) * (radius + scale) * ff
            y = cy - math.cos(posOffset * pi * 2.0) * (radius + scale) * ff
            nextVec = Vec(x, y)
            if vec is None or distSq(vec, nextVec) > 0.0005:
                vec = nextVec
                channelPath.append(vec)
            # rr*=02.51
            # if radius < 0.2:
            radius *= 1.1
            # if radius < 1000000:
            #   radius = math.pow(radius, 1.2)
            # if scale < 0.001:
            # break
            # list.append(Vec(cx, cy))
        if fProgrTick % 2 == 0:
            channelPath.reverse()
        # list.append(Vec(cx, cy))
        paths.append(channelPath)

    return paths


def genShapes2(fftBandsIn, t, fftOffset, fftBins, rangeStep = 32):
    # t = 457.
    # t *=0.1
    # b = scaledLvlCl(fftBandsIn[0][8]-0.002, 0.0, 1.0)
    # if b > 0.6:
    #   # return [[],[]]
    #   t *= 3.0

    fProgr, fProgrTick = tmToBeat(t, u_bpm, 4)
    fProgrQ, fProgrTickQ = tmToBeat(t, u_bpm, 2)
    pos1 = seededRndFloat(fProgrTick)
    pos2 = seededRndFloat(fProgrTick + 1)
    posBegin = pos1 + (pos2 - pos1) * fProgr

    cx = seededRndFloat(7 * math.floor(fProgrTickQ)) * 1.6
    cy = seededRndFloat(7 * math.floor(fProgrTickQ) + 1) * 0.8
    cx1 = seededRndFloat(fProgrTick) * 1.6
    cy1 = seededRndFloat(fProgrTick + 214124)
    cx2 = seededRndFloat(fProgrTick + 1) * 1.6
    cy2 = seededRndFloat(fProgrTick + 214124 + 1)
    cx = cx1 + (cx2 - cx1) * fProgr
    cy = cy1 + (cy2 - cy1) * fProgr
    cx *= 0.9
    cy *= 0.9
    # cx = 0
    # cy = 0

    paths = []
    for j in range(2):
        # channelPath = []
        channelPath = array.array("f", [20.0]) * rangeStep * 2
        radius = (1.0 - abs(fProgrQ * 2.0 - 1.0)) * 1.6
        minOneOne = fProgrQ * 2.0 - 1.0
        radius = (1.0 - abs(minOneOne)) * 0.02 + 0.01
        fftBands = fftBandsIn[j]
        if fftOffset + fftBins >= len(fftBands):
            fftBands = fftBands[-fftBins:]
        else:
            fftBands = fftBands[fftOffset: (fftOffset + fftBins)]

        numBands = len(fftBands)
        fftStepSize = numBands / rangeStep
        pos = (posBegin + j * 0.5 + (t * 2.5 * ((fProgrTick % 2) * 2 - 1))) % 1.0
        numPts = 0
        for i in range(0, rangeStep):
            posOffset = pos + i / (rangeStep - 1.0)
            bandIdx = math.floor(i * fftStepSize)
            if fProgrTick % 2 == 0:
                bandIdx = numBands - 1 - bandIdx
            scale = 0.0
            if i > 0 and bandIdx >= 0 and bandIdx < numBands:
                scaleRng = 128/(bandIdx*bandIdx+16.0)
                scale += scaledLvlCl(fftBands[bandIdx], 0, 0.5+scaleRng)
                # scale *= 0.25
                # scale *= 2.1
                # radius *= 1.0+scale * 0.12
            ff = 1.0
            x = cx + math.sin(posOffset * pi * 2.0) * (radius + scale) * ff
            y = cy - math.cos(posOffset * pi * 2.0) * (radius + scale) * ff
            if numPts == 0 or distVecSq(channelPath[numPts*2], channelPath[numPts*2+1], x, y) > 0.0005:
                channelPath[numPts*2]   = x
                channelPath[numPts*2+1] = y
                numPts += 1
            radius *= 1.0 + (0.12*32.0/rangeStep)
        channelPath = channelPath[0:numPts*2]
        paths.append(channelPath)
    # x1 = paths[0][0]
    # y1 = paths[0][1]
    # paths[0][0] = paths[1][0]
    # paths[0][1] = paths[1][1]
    # paths[1][0] = x1
    # paths[1][1] = y1

    # note: flips x and y coordinates
    if fProgrTick % 2 == 0:
        paths[0].reverse()
        paths[1].reverse()
    return paths

def pathgen_fl32_shaper(fftBandsIn, t):
    numSteps = 1024 + 1
    axisX = [-1.0, 0.0, 1.0, 0.0]
    axisY = [0.0, -1.0, 0.0, 1.0]
    numPaths = 3
    pathList = [
        axisX, axisY
    ]
    for i in range(numPaths):
        path = array.array("f", [0.0]) * numSteps * 2
        for step in range(numSteps):
            x = (step / (numSteps - 1.0));
            x *= 1.
            # y = math.sin(x * pi * 2.0)
            path[step * 2 + 0] = x
            y = 0;
            if i == 0:
                x1 = 0.1+999.9*(1.0-math.pow(x, 0.05))
                y = x1
            elif i == 1:
                ValPowd = 0.
                if x > 1.E-12:
                    ValLogBase = math.log(x)
                    ValPowd = math.exp(ValLogBase*0.1)
                    ValPowd = .5-.5*math.cos(ValPowd * math.pi)
                    # ValPowd = math.exp(ValLogBase*abs(ValLogBase))
                # ValPowd = math.pow(x, 0.05)
                x1 = 0.1+999.9*(1.0-ValPowd)
                y = x1
                # y = math.pow(x*1., .1)
            # elif i == 1:
            #     y = math.cos(math.pow(x*1., .1) * math.pi)
            # elif i == 2:
            #     y = 1. - (.5 - .5 * math.cos(math.pow(x*1., .1) * math.pi))+0.02
            # elif i == 3:
            else:
                y = 1000.0 - 999.9 * (.5 - .5 * math.cos(math.pow(x*1., .1) * math.pi))
            y /= 1000.0
            path[step * 2 + 0] = x
            # path[step * 2 + 0] = 0.1*i + x
            path[step * 2 + 1] = y
        pathList.append(path)
    return pathList
def pathgen_fl32_expr(fftBandsIn, t):
    numSteps = 1024 + 1
    axisX = [-1.0, 0.0, 1.0, 0.0]
    axisY = [0.0, -1.0, 0.0, 1.0]
    numPaths = 2
    pathList = [
        axisX, axisY
    ]
    lfoAmount = 0.0 - (t*0.1)%0.94
    for i in range(numPaths):
        path = array.array("f", [0.0]) * numSteps * 2
        for step in range(numSteps):
            x = (step / (numSteps - 1.0));
            x *= 2.
            x -= 1.
            # y = math.sin(x * pi * 2.0)
            # path[step * 2 + 0] = x
            y = 0;
            if i <= 1:
                dVoiceLfoUni = 1.0-x
                if lfoAmount < 0.0:
                    dLfoShapeExp = 1.0 + dVoiceLfoUni * -lfoAmount * 16.;
                else:
                    dLfoShapeExp = 1.0 / (1.0 + dVoiceLfoUni * lfoAmount * 16.);
                if i == 0:
                    y = 1-math.pow(1-abs(x), 1/2.0);
                if i == 1:
                    y = 1-math.sqrt(1-abs(x))#math.pow(abs(x), 2.0);
                    y = abs(x)**24.2
                #     y = 0.0 if dVoiceLfoUni < 0.00001 else math.exp(math.log(dVoiceLfoUni*dVoiceLfoUni) * dLfoShapeExp);
                # else:
                #     y = 0.0 if dVoiceLfoUni < 0.00001 else math.exp(math.log(dVoiceLfoUni) * dLfoShapeExp);
            path[step * 2 + 0] = x
            path[step * 2 + 1] = y
        pathList.append(path)
    return pathList

def pathgen_fl32_path(fftBandsIn, t):
    pathList = []
    t *= 0.025
    # t *= 0.00025 + 0.00002*(0.5+0.5*math.sin(t*0.3))
    # t*=1.
    nBinsTotal = len(fftBandsIn[0])
    nShapes = 12
    nFirstBin = 0
    perShapeBins = max(1, round(nBinsTotal / nShapes * 8))
    for i in range(nShapes):
        rangeStep=max(2, 48-math.floor(i*0.25))
        shapeList = genShapes2(
            fftBandsIn, 500.42 * i + t, nFirstBin, fftBins=int(nBinsTotal/2), rangeStep=rangeStep
        )
        pathList += shapeList
        nFirstBin += 4
        if nFirstBin >= nBinsTotal - 1:
            nFirstBin = 0
    return pathList
    # ctrlPts = array.array("f", [0.0]) * 1024
    # return ctrlPts

def distSq(v1, v2):
    dx = v2.x - v1.x
    dy = v2.y - v1.y
    return dx * dx + dy * dy


def distVecSq(x1, y1, x, y):
    dx = x - x1
    dy = y - y1
    return dx * dx + dy * dy


def distCt(v1):
    dx = v1.x
    dy = v1.y
    return dx * dx + dy * dy

def compareDistCt(v1, v2):
    dx = distCt(v1)
    dy = distCt(v2)
    return dx - dy

def removePointsTooClose(listIn):
    firstV = listIn[0]
    list2 = []
    list2.append(firstV)
    for i, vec in enumerate(listIn[1:]):
        if distSq(firstV, vec) > 0.001:
            list2.append(vec)
            firstV = vec
    return list2

def pathGen_multiShapes(fftBandsIn, t):
    pathList = []
    # t*=0.0005
    t *= 0.25
    # t*=1.
    nBinsTotal = len(fftBandsIn[0])
    nShapes = 12
    nFirstBin = 0
    for i in range(nShapes):
        shapeList = genShapes(
            fftBandsIn, 500.42 * i + t, nFirstBin, nBinsTotal - nFirstBin, rangeStep=32
        )
        pathList += shapeList
        nFirstBin += round(nBinsTotal / nShapes )
        if nFirstBin >= nBinsTotal - 1:
            nFirstBin = 0
    return pathList


def pathGen_multiShapes2(fftBandsIn, t):
    pathList = []
    t *= 0.25
    nBinsTotal = len(fftBandsIn[0])
    nFirstBin = 0
    for i in range(7):
        shapeList = genShapes(
            fftBandsIn, 500.42 * i + t, nFirstBin, nBinsTotal - nFirstBin
        )
        pathList += shapeList
        nFirstBin += round(nBinsTotal / 12)
        if nFirstBin >= nBinsTotal - 1:
            nFirstBin = 0
    return pathList


def pathGen_trippyKaleidoscope3(listIn, t):
    t *= 0.1
    ftimestep = 3.10
    fProgr = t / ftimestep
    fProgrTick = int(math.floor(fProgr))
    fProgr = fProgr - fProgrTick
    pos1 = seededRndFloat(fProgrTick)
    pos2 = seededRndFloat(fProgrTick + 1)
    pos = pos1 + (pos2 - pos1) * fProgr
    fsin = math.sin(t * 20.1) + 1.0
    # rangeStep = 5+math.floor(fsin*46)
    rangeStep = 4
    cx = seededRndFloat(7 * math.floor(t * 6.3)) * 0.9
    cy = seededRndFloat(7 * math.floor(t * 3.3) + 1) * 0.5

    path = []
    radius = (t * 8.0) % 1.4
    radius *= radius

    for i in range(0, int(rangeStep), 1):
        # t+=i*0.12
        posOffset = pos + i / (rangeStep - 1.0)
        # radius = 0.75+0.25*math.sin(t*-4+i/rangeStep*0.25)
        x = cx + math.sin(posOffset * pi * 2.0) * radius
        y = cy - math.cos(posOffset * pi * 2.0) * radius
        path.append(Vec(x, y))
        rr = (((t + i) * 2.12) % 1.0) * 0.4 + 0.1
        rr *= 0.2
        for j in range(0, 4, 1):
            x += seededRndFloat(fProgrTick + i + 1234123) * rr
            y += seededRndFloat(fProgrTick + i + 423525) * rr
            path.append(Vec(x, y))
            rr *= rr
        radius *= radius
    return [path]


def pathGen_randomizePathEye2(listIn, t):
    path = []
    strength = 2.0
    t2 = t * 6.3
    ti2 = math.floor(t2)
    tp2 = t2 - ti2
    pos1 = seededRandom(ti2)
    pos2 = seededRandom(ti2 + 1)
    pos = pos1 + (pos2 - pos1) * smoothstep(tp2)
    # t += frameNum*1000.0
    # af = ((t*324.0)%3.0)/7.5
    # af = (t*4.0)%1.0
    af = 1 / 10.0
    fRange = 0.2
    for i in range(0, 32, 1):
        fScale = (i + 16) * af
        minAngle = pos - fRange
        maxAngle = pos + fRange
        posOffset = minAngle + (maxAngle - minAngle) * random.random()
        t2 = clamp((i) / 60.0, 0.0, 1.0)
        fScale2 = clamp(strength * fScale * af, 0.2 * t2, 1.02)
        x = math.sin(posOffset * pi * 2.0) * fScale2
        y = math.cos(posOffset * pi * 2.0) * fScale2
        path.append(Vec(x, y))
    # list = list * 8
    return [path]


def nextPoint(listIn):
    for pt in listIn:
        yield pt


def pathGen_randomizePath2(listIn, t):
    path = []
    rnd = random.Random()
    rnd.seed(12312 + math.floor(t * 0.07))
    rnd.shuffle(listIn)
    for pt in nextPoint(listIn):
        path.append(pt)

    return [path]


def pathGen_circleFFTMono(fftBandsIn, t):
    cx = 0.0
    cy = 0.0
    path = []
    radius = 0.8
    rangeStep = 1024
    fftBands = fftBandsIn[0]
    numBands = len(fftBands)
    for i in range(0, int(rangeStep), 1):
        posOffset = (i / (rangeStep - 1.0) + t * 1.0) % 1.0
        scale = radius
        bandIdx = numBands - 1 - round(posOffset * (numBands - 1))
        if bandIdx >= 0 and bandIdx < numBands:
            scale *= scaledLvlCl(fftBands[bandIdx], 0.5, 1.5)

        x = cx + math.sin(posOffset * pi * 2.0) * scale
        y = cy - math.cos(posOffset * pi * 2.0) * scale
        path.append(Vec(x, y))
    return [path]


def circleFFT_NStepsStereo(fftBandsIn, t, rangeStep, radiusMin=0.2, radiusMax=1.2, cx = 0.0, cy = 0.0, scX = 1.0, scY = 1.0):
    path = []
    fftBands = fftBandsIn[0] + fftBandsIn[1][::-1]
    numBands = len(fftBands)
    for i in range(0, int(rangeStep), 1):
        posOffset = (i / (rangeStep - 1.0) + t * 3.0) % 1.0
        scale = radiusMin
        bandIdx = numBands - 1 - round(posOffset * (numBands - 1))
        if bandIdx >= 0 and bandIdx < numBands:
            scale = scaledLvlCl(fftBands[bandIdx], radiusMin, radiusMax)
            # print(i, bandIdx)

        x = cx + math.sin(posOffset * pi * 2.0) * scale * scX
        y = cy - math.cos(posOffset * pi * 2.0) * scale * scY
        path.append(Vec(x, y))
    return [path]


def pathGen_circleFFT1024StepsStereo(fftBandsIn, t):
    return circleFFT_NStepsStereo(fftBandsIn, t, 1024, 0.4, 1.3)
def pathGen_circleFFT1024StepsStereoBig(fftBandsIn, t):
    return circleFFT_NStepsStereo(fftBandsIn, t, 1024, 0.02, 1.3, cy = -0.0, scX = 2.0)
def pathGen_circleFFT2xStepsStereoBig(fftBandsIn, t):
    return circleFFT_NStepsStereo(fftBandsIn, t, len(fftBandsIn[0])*2, 0.02, 1.3, cy = -0.2, scX = 2.0)
def pathGen_circleFFT1024StepsStereoBig2(fftBandsIn, t):
    return circleFFT_NStepsStereo(fftBandsIn, t, 1024, 0.01, 2.6, cy = -0.1, scX = 1.0)


def pathGen_circleFFT64StepsStereo(fftBandsIn, t):
    return circleFFT_NStepsStereo(fftBandsIn, t, 64)


def pathGen_circleFFTStepBandsStereo(fftBandsIn, t):
    cx = 0.0
    cy = 0.0
    path = []
    radius = 0.8
    rangeStep = len(fftBandsIn[0])
    fftBands = fftBandsIn[0] + fftBandsIn[1][::-1]
    numBands = len(fftBands)
    for i in range(0, int(rangeStep), 1):
        posOffset = (i / (rangeStep - 1.0) + t * 0.3) % 1.0
        scale = radius
        bandIdx = numBands - 1 - round(posOffset * (numBands - 1))
        if bandIdx >= 0 and bandIdx < numBands:
            scale *= scaledLvlCl(fftBands[bandIdx], 0.5, 1.5)

        x = cx + math.sin(posOffset * pi * 2.0) * scale
        y = cy - math.cos(posOffset * pi * 2.0) * scale
        path.append(Vec(x, y))
    return [path]


def pathGen_circle(fftBands, t):
    cx = 0.0
    cy = 0.0
    path = []
    radius = 0.8
    rangeStep = 1024
    for i in range(0, int(rangeStep), 1):
        posOffset = i / (rangeStep - 1.0) + (t * 0.3) % 1.0
        x = cx + math.sin(posOffset * pi * 2.0) * radius
        y = cy - math.cos(posOffset * pi * 2.0) * radius
        path.append(Vec(x, y))
    return [path]


def pathGen_dunes(listIn, t):
    path = []
    r = 12
    rnd = random.Random()
    fProgrQ, fProgrTickQ = tmToBeat(t, u_bpm, 4)
    fProgr, fProgrTick = tmToBeat(t, u_bpm, 2)
    fProgrL, fProgrTickL = tmToBeat(t, u_bpm, 32)
    fProgrL = max(0.0, min(1.0, (fProgrL - 0.9) * 10.0))
    # a = t
    # s = a-math.floor(a)
    rnd.seed(math.floor(fProgrTickQ ^ 239874))
    rnd.shuffle(listIn)
    h = 0.04 + 0.6 * (fProgrQ + fProgrL * 0.7)
    # for pt in nextPoint(listIn):
    #    list.append(pt)
    phase = fProgrTick
    for i in range(0, r, 1):
        f = i / (r - 1.0)
        x = (f * 2.0 - 1.0) * 3.0
        y = (
            0.3
            + math.sin((f + phase) * 0.9 * pi * 2.0) * h
            + (rnd.random() * 2.0 - 1.0) * 0.15
        )
        ptOut = Vec(x, y)
        # ptOut.x += (random.random())*fScale*strength
        # ptOut.y += (random.random())*fScale*strength
        path.append(ptOut)
    return [path]


def pathGen_dunesOld(listIn, t):
    t *= 1000.005
    path = []
    r = 12
    rnd = random.Random()
    a = t * 0.001
    s = a - math.floor(a)
    rnd.seed(math.floor(a))
    rnd.shuffle(listIn)
    h = 0.04 + 0.6 * s
    # for pt in nextPoint(listIn):
    #    list.append(pt)
    phase = math.floor((t * 0.2) % 1000.0) / 1000.0
    for i in range(0, r, 1):
        f = i / (r - 1.0)
        x = (f * 2.0 - 1.0) * 3.0
        y = (
            math.sin((f + phase) * 0.9 * pi * 2.0) * h
            + (rnd.random() * 2.0 - 1.0) * 0.1
        )
        ptOut = Vec(x, y)
        # ptOut.x += (random.random())*fScale*strength
        # ptOut.y += (random.random())*fScale*strength
        path.append(ptOut)
    return [path]


def pathGen_dunes2(listIn, t):  # E4DDBD68
    path = []
    t *= 0.003
    rnd = random.Random()
    rnd.seed(1 + math.floor(t * 0.002))
    time = t * 0.3
    seedB = math.floor(time)
    rndB = random.Random(seedB)
    progress = time - math.floor(time)
    progress = 1 - abs(progress * 2 - 1)  # linear tri

    # rnd.shuffle(listIn)
    # for pt in nextPoint(listIn):
    #    list.append(pt)

    r = 64
    a = t * 0.5
    s = a - math.floor(a)
    h = 0.7  # 0.04+0.6*s
    phase = t * 120
    fRandA = 1
    # fRandA = 1
    fRandB = 0
    for i in range(0, r, 1):
        fRand = fRandA + (progress) * (fRandB - fRandA)
        f = i / (r - 1.0)
        x = (f * 2.0 - 1.0) * 4
        y = 0.0 + math.sin((f + phase) * pi * 2.0) * h + (fRand * 2.0 - 1.0) * 0.125
        ptOut = Vec(x, y)
        # if i < len(listIn):
        #     ptOut.x += listIn[0].x
        #     ptOut.y += listIn[0].y
        # ptOut.x += (random.random())*fScale*strength
        # ptOut.y += (random.random())*fScale*strength
        path.append(ptOut)
    return [path]


def pathGen_kaleidoMix(fftBandsIn, t):
    paths = []
    paths.extend(pathGen_trippyKaleidoscope2(fftBandsIn, t))
    paths.extend(pathGen_trippyKaleidoscope3(fftBandsIn, t))
    return paths


def pathGen_kaleidoSpectrum(fftBandsIn, t):

    paths = []
    paths.extend(pathGen_triangleExpanding(fftBandsIn, t))
    paths.extend(pathGen_testProcFFT(fftBandsIn, t))
    paths.extend(pathGen_trippyKaleidoscope5(fftBandsIn, t))
    # for i in range(min(len(fftBandsIn), len(list))):
    #     list[i].y*=scaledLvlCl(fftBandsIn[i], 0.1, 3.1)
    return paths


def pathGen_randomizePath1(listIn, t):
    path = []
    i = 0
    l1 = len(listIn) - 1
    t = t * 0.001
    for pt in nextPoint(listIn):
        path.append(Vec(pt.x, pt.y * ((0.6 + (t % 2.0) * 0.125))))
        # list.append(Vec(pt.y, pt.x*((0.6+((t*0.94)%2.0)*0.125))))
        i += 1
    return [path]


def pathGen_test(listIn, t):
    path = None
    if listIn is not None:
        path = []
        for pt in listIn[:-1]:
            path.append(pt)
    return [path]
