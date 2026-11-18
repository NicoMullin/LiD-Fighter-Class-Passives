"""Write harness.c: the C part from harness.in.c plus generated assembly.

The harness stands in for BrgGame-Steam.exe. Its fake skill loop, status reset
and pause map are written with the same machine-code bytes the DLL looks for in
the real game, so the DLL finds and patches them exactly as it would there.

Usage: python tests/make_harness.py OUTPUT.c
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TYPES = [('th', 'SKLTP_SEARCHUP_ITEM'), ('spy', 'SKLTP_ADVENTURE'), ('patrol', 'SKLTP_PATROL_WOT'),
         ('lift', 'SKLTP_SEARCHLIFT_SPIRITUP_K7'), ('item', 'SKLTP_SEARCHITEM_MONEYUP_K7'), ('hpup', 'SKLTP_HPUP')]


def asm(lines):
    return ''.join('    "%s\\n"\n' % l.replace('"', '\\"') for l in lines)


strings = ['.section .rdata,"dr"']
for sym, s in TYPES:
    strings += ['.balign 16', '.quad 0', f'.globl str_{sym}', f'str_{sym}:',
                '.short ' + ','.join(str(ord(c)) for c in s) + ',0']
strings += ['.text']


def case(sym, anchor=False):
    lines = ['movq (%r14), %rcx', '.byte 0x48,0x8D,0x15', f'.long str_{sym} - (. + 4)',
             '.byte 0x48,0x83,0xC1,0x34,0x48,0x03,0xCF,0xE8', '.long fake_cmp - (. + 4)']
    if anchor:
        lines += ['.byte 0x85,0xC0,0x74,0x0C,0x81,0x4E,0x70,0x00,0x00,0x10,0x00,0xE9', '.long 9f - (. + 4)']
    else:
        lines += ['.byte 0x85,0xC0']
    return lines


loop = ['.globl fake_loop', 'fake_loop:', 'pushq %rsi', 'pushq %rdi', 'pushq %r14', 'subq $0x20, %rsp',
        'movq %rcx, %rsi', 'movq %rdx, %r14', 'xorl %edi, %edi']
loop += case('th', True)
for s in ('spy', 'patrol', 'lift', 'item'):
    loop += case(s)
loop += ['9:', 'addq $0x20, %rsp', 'popq %r14', 'popq %rdi', 'popq %rsi', 'ret']

init = ['.globl fake_init', 'fake_init:', 'pushq %rbx', 'movq %rcx, %rbx',
        '.byte 0x8B,0x43,0x70,0x25,0x9F,0x20,0x00,0xFE,0x0F,0xBA,0xE8,0x07,0x89,0x43,0x70', 'popq %rbx', 'ret']

render = ['.globl fake_render', 'fake_render:', 'pushq %rbx', 'pushq %rsi', 'pushq %rdi', 'pushq %r12',
          'pushq %r13', 'subq $0x70, %rsp', 'movq %rcx, %rbx', 'movq %rdx, %rdi', 'movq %r9, %r12',
          'movq %r8, %rax', 'movl $1, %r13d',
          '.byte 0xF7,0x80,0xCC,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x0F,0x85', '.long 8f - (. + 4)',
          '.byte 0x48,0x8B,0x01,0x4C,0x89,0x6C,0x24,0x60,0xFF,0x90,0x38,0x10,0x00,0x00', 'movl %eax, (%r12)',
          'movq %rbx, %rcx', '.byte 0x48,0x8B,0x01,0xFF,0x90,0x40,0x10,0x00,0x00', 'movl %eax, 4(%r12)',
          'movq %rbx, %rcx', '.byte 0x48,0x8B,0x01,0xFF,0x90,0x48,0x10,0x00,0x00', 'movl %eax, 8(%r12)',
          'movq %rbx, %rcx', '.byte 0x48,0x8B,0x01,0x41,0x0F,0x44,0xF5,0xFF,0x90,0x50,0x10,0x00,0x00',
          'movl %eax, 12(%r12)',
          'xorl %eax, %eax', 'jmp 7f', '8:', 'movl $-1, %eax', '7:', 'addq $0x70, %rsp',
          'popq %r13', 'popq %r12', 'popq %rdi', 'popq %rsi', 'popq %rbx', 'ret']

# the HUD minimap's Treasure Hunter test, where the DLL reads the flag word's place
# (fighter+0x47BC, as in game 1.93): mov rax,[rcx+0x2D4C] / test dword [rax+0x47BC],0x100000
hudmap = ['.globl fake_hudmap', 'fake_hudmap:',
          '.byte 0x48,0x8B,0x81,0x4C,0x2D,0x00,0x00,0xF7,0x80,0xBC,0x47,0x00,0x00,0x00,0x00,0x10,0x00',
          'setne %al', 'movzbl %al, %eax', 'ret']

def hexbytes(h):
    return '.byte ' + ','.join('0x' + h[i:i + 2] for i in range(0, len(h), 2))


# The minimap's paint call and spray call in the game's byte shapes (game 1.93), and the
# minimap's function table holding them at 0x450 / 0x350. The paint call only counts its
# calls; the spray call hands over to c_add in harness.in.c. The bytes after each ret are the
# layout pieces the DLL checks for (never run).
update = ['.globl fake_update', 'fake_update:',
          hexbytes('488bc4488950105741544881ec180100004883b9bc000000004c8be2488bf90f84'),
          '.long 6f - (. + 4)', '6:', 'incl update_calls(%rip)',
          'addq $0x118, %rsp', 'popq %r12', 'popq %rdi', 'ret',
          hexbytes('8b87cc000000a801' '488b8740010000423b6c3824' '4b8b443e1c' '4d6bff2c')]
add = ['.globl fake_add', 'fake_add:',
       hexbytes('48895c2408574883ec608b81940000004863b91c010000488bd9'),
       'addq $0x60, %rsp', 'popq %rdi', 'movq 8(%rsp), %rbx', 'jmp c_add',
       hexbytes('48038314010000' '486bc01c' 'f30f114804' 'f30f117818')]
table = ['.section .rdata,"dr"', '.balign 16', '.quad 0', '.globl fake_vt', 'fake_vt:',
         '.fill 0x350/8, 8, 0', '.quad fake_add', '.fill (0x450-0x358)/8, 8, 0', '.quad fake_update',
         '.fill 8, 8, 0', '.text']

body = (HERE / 'harness.in.c').read_text().replace(
    '@ASM@', asm(strings + loop + init + render + hudmap + update + add + table))
Path(sys.argv[1]).write_text(body)
