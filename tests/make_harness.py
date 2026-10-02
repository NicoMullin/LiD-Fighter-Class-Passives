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

body = (HERE / 'harness.in.c').read_text().replace('@ASM@', asm(strings + loop + init + render))
Path(sys.argv[1]).write_text(body)
