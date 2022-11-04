

def toU64Array(strDataEnc):
        strDataEnc += b'\x00'   
        while len(strDataEnc) % 8 != 0:
            strDataEnc += b'\x00'
        u32Array = memoryview(strDataEnc).cast('Q')
        outputStr = ''
        for i,u in enumerate(u32Array):
            if not i%6:
                outputStr += '\n';
            outputStr += "0x%016X,"%u
        return outputStr[1:-1]
def toU32Array(strDataEnc):
        strDataEnc += b'\x00'   
        while len(strDataEnc) % 4 != 0:
            strDataEnc += b'\x00'
        u32Array = memoryview(strDataEnc).cast('I')
        return ",".join(["0x%08X"%u for u in u32Array])
def toU8Array(strDataEnc):
        strDataEnc += b'\x00'   
        # while len(strDataEnc) % 4 != 0:
        #     strDataEnc += b'\x00'
        u8Array = memoryview(strDataEnc).cast('B')
        return ",".join(["0x%02X"%u for u in u8Array])
def toHexString(strDataEnc):
        strDataEnc += b'\x00'   
        # while len(strDataEnc) % 4 != 0:
        #     strDataEnc += b'\x00'
        charArray = memoryview(strDataEnc).cast('c')
        return "".join(["\\x%02X" % ord(c) for c in charArray])
