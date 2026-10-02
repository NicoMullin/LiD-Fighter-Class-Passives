"""A small UE3 (LET IT DIE, package 861) bytecode walker: disk bytes -> tokens with memory offsets.

Object references are 4 bytes on disk and 8 in memory; names are 8 in both.
Jump targets and skip counts are measured in memory bytes.
"""
import struct

OBJ_DISK, OBJ_MEM = 4, 8


class Walker:
    def __init__(self, code: bytes, name_of=None, obj_of=None):
        self.code = code
        self.pos = 0          # disk
        self.mem = 0          # memory
        self.lines = []
        self.name_of = name_of or (lambda i: f"name#{i}")
        self.obj_of = obj_of or (lambda i: f"obj#{i}")

    # primitives -------------------------------------------------------
    def u8(self):
        v = self.code[self.pos]; self.pos += 1; self.mem += 1; return v

    def u16(self):
        v = struct.unpack_from("<H", self.code, self.pos)[0]; self.pos += 2; self.mem += 2; return v

    def i32(self):
        v = struct.unpack_from("<i", self.code, self.pos)[0]; self.pos += 4; self.mem += 4; return v

    def obj(self):
        v = struct.unpack_from("<i", self.code, self.pos)[0]; self.pos += OBJ_DISK; self.mem += OBJ_MEM
        return self.obj_of(v)

    def name(self):
        i, n = struct.unpack_from("<ii", self.code, self.pos); self.pos += 8; self.mem += 8
        return self.name_of(i)

    def cstr(self):
        end = self.code.index(b"\0", self.pos)
        s = self.code[self.pos:end].decode("latin-1")
        n = end + 1 - self.pos
        self.pos += n; self.mem += n
        return s

    # expressions --------------------------------------------------------
    def params(self):
        out = []
        while self.code[self.pos] != 0x16:
            out.append(self.expr())
        self.u8()
        return out

    def expr(self):
        t = self.u8()
        if t == 0x00: return f"local {self.obj()}"
        if t == 0x01: return f"inst {self.obj()}"
        if t == 0x02: return f"default {self.obj()}"
        if t == 0x04: return f"return({self.expr()})"
        if t == 0x06: return f"jump 0x{self.u16():x}"
        if t == 0x07:
            to = self.u16(); return f"jumpifnot 0x{to:x} ({self.expr()})"
        if t == 0x0B: return "nothing"
        if t == 0x0E: return f"eatreturn {self.obj()} {self.expr()}"
        if t == 0x0F: return f"let {self.expr()} = {self.expr()}"
        if t == 0x10: return f"{self.expr()}[{self.expr()}]" if False else self._dynelem()
        if t == 0x14: return f"letbool {self.expr()} = {self.expr()}"
        if t == 0x16: return "endparms"
        if t == 0x17: return "self"
        if t in (0x19, 0x12):
            left = self.expr(); skip = self.u16(); field = self.obj(); size = self.u8()
            right = self.expr()
            return f"{'ctx' if t == 0x19 else 'classctx'}[{left} .skip=0x{skip:x} field={field} sz={size}] {right}"
        if t == 0x1B:
            n = self.name(); return f"vcall {n}({', '.join(self.params())})"
        if t == 0x1C:
            f = self.obj(); return f"fcall {f}({', '.join(self.params())})"
        if t == 0x1D: return f"int {self.i32()}"
        if t == 0x1F: return f'"{self.cstr()}"'
        if t == 0x20: return f"objconst {self.obj()}"
        if t == 0x21: return f"nameconst {self.name()}"
        if t == 0x24: return f"byte {self.u8()}"
        if t == 0x25: return "0"
        if t == 0x26: return "1"
        if t == 0x27: return "true"
        if t == 0x28: return "false"
        if t == 0x2A: return "none"
        if t == 0x2C: return f"intb {self.u8()}"
        if t == 0x2D: return f"boolvar {self.expr()}"
        if t == 0x2E:
            c = self.obj(); return f"cast<{c}>({self.expr()})"
        if t == 0x35:
            field = self.obj(); struct_ = self.obj(); a = self.u8(); b = self.u8()
            return f"({self.expr()}).{field}@{struct_}[{a},{b}]"
        if t == 0x36: return f"len({self.expr()})"
        if t == 0x38:
            k = self.u8(); return f"pcast{k}({self.expr()})"
        if t == 0x3A: return f"returnnothing {self.obj()}"
        if t == 0x48: return f"localout {self.obj()}"
        if t == 0x4A: return "emptyparm"
        if t == 0x53: return "END"
        if t == 0x55:
            arr = self.expr(); skip = self.u16(); item = self.expr(); self.u8()
            return f"{arr}.additem(skip=0x{skip:x}, {item})"
        if 0x60 <= t <= 0x6F:
            idx = ((t - 0x60) << 8) | self.u8()
            return f"native{idx}({', '.join(self.params())})"
        if t >= 0x70:
            return f"native{t}({', '.join(self.params())})"
        raise ValueError(f"unknown token 0x{t:02x} at disk {self.pos - 1}")

    def _dynelem(self):
        index = self.expr(); arr = self.expr()
        return f"{arr}[{index}]"

    def run(self):
        while self.pos < len(self.code):
            d, m = self.pos, self.mem
            text = self.expr()
            self.lines.append((d, m, text))
            if text == "END":
                break
        return self.lines
