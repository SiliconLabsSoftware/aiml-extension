# automatically generated by the FlatBuffers compiler, do not modify

# namespace: tflite

import flatbuffers
from flatbuffers.compat import import_numpy
np = import_numpy()

class SignatureDef(object):
    __slots__ = ['_tab']

    @classmethod
    def GetRootAs(cls, buf, offset=0):
        n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, offset)
        x = SignatureDef()
        x.Init(buf, n + offset)
        return x

    @classmethod
    def GetRootAsSignatureDef(cls, buf, offset=0):
        """This method is deprecated. Please switch to GetRootAs."""
        return cls.GetRootAs(buf, offset)
    @classmethod
    def SignatureDefBufferHasIdentifier(cls, buf, offset, size_prefixed=False):
        return flatbuffers.util.BufferHasIdentifier(buf, offset, b"\x54\x46\x4C\x33", size_prefixed=size_prefixed)

    # SignatureDef
    def Init(self, buf, pos):
        self._tab = flatbuffers.table.Table(buf, pos)

    # SignatureDef
    def Inputs(self, j):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            x = self._tab.Vector(o)
            x += flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
            x = self._tab.Indirect(x)
            from tflite.TensorMap import TensorMap
            obj = TensorMap()
            obj.Init(self._tab.Bytes, x)
            return obj
        return None

    # SignatureDef
    def InputsLength(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            return self._tab.VectorLen(o)
        return 0

    # SignatureDef
    def InputsIsNone(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        return o == 0

    # SignatureDef
    def Outputs(self, j):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            x = self._tab.Vector(o)
            x += flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
            x = self._tab.Indirect(x)
            from tflite.TensorMap import TensorMap
            obj = TensorMap()
            obj.Init(self._tab.Bytes, x)
            return obj
        return None

    # SignatureDef
    def OutputsLength(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            return self._tab.VectorLen(o)
        return 0

    # SignatureDef
    def OutputsIsNone(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        return o == 0

    # SignatureDef
    def SignatureKey(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(8))
        if o != 0:
            return self._tab.String(o + self._tab.Pos)
        return None

    # SignatureDef
    def SubgraphIndex(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(12))
        if o != 0:
            return self._tab.Get(flatbuffers.number_types.Uint32Flags, o + self._tab.Pos)
        return 0

def SignatureDefStart(builder): builder.StartObject(5)
def Start(builder):
    return SignatureDefStart(builder)
def SignatureDefAddInputs(builder, inputs): builder.PrependUOffsetTRelativeSlot(0, flatbuffers.number_types.UOffsetTFlags.py_type(inputs), 0)
def AddInputs(builder, inputs):
    return SignatureDefAddInputs(builder, inputs)
def SignatureDefStartInputsVector(builder, numElems): return builder.StartVector(4, numElems, 4)
def StartInputsVector(builder, numElems):
    return SignatureDefStartInputsVector(builder, numElems)
def SignatureDefAddOutputs(builder, outputs): builder.PrependUOffsetTRelativeSlot(1, flatbuffers.number_types.UOffsetTFlags.py_type(outputs), 0)
def AddOutputs(builder, outputs):
    return SignatureDefAddOutputs(builder, outputs)
def SignatureDefStartOutputsVector(builder, numElems): return builder.StartVector(4, numElems, 4)
def StartOutputsVector(builder, numElems):
    return SignatureDefStartOutputsVector(builder, numElems)
def SignatureDefAddSignatureKey(builder, signatureKey): builder.PrependUOffsetTRelativeSlot(2, flatbuffers.number_types.UOffsetTFlags.py_type(signatureKey), 0)
def AddSignatureKey(builder, signatureKey):
    return SignatureDefAddSignatureKey(builder, signatureKey)
def SignatureDefAddSubgraphIndex(builder, subgraphIndex): builder.PrependUint32Slot(4, subgraphIndex, 0)
def AddSubgraphIndex(builder, subgraphIndex):
    return SignatureDefAddSubgraphIndex(builder, subgraphIndex)
def SignatureDefEnd(builder): return builder.EndObject()
def End(builder):
    return SignatureDefEnd(builder)