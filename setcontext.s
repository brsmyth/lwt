/*
	
Copyright 2016-2020 Cazamar Systems

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#if defined(__arm__)
        .syntax unified
	.global xsetcontext
	.text

	MCONTEXT_ARM_R4=48
	MCONTEXT_ARM_SP=84
	MCONTEXT_ARM_LR=88
	MCONTEXT_ARM_PC=92
	MCONTEXT_ARM_R0=32
	/* int setcontext (const ucontext_t *ucp) */
	
xsetcontext:
	mov        r4, r0
	
	/* Loading r0-r3 makes makecontext easier.  */
	add     r14, r4, #MCONTEXT_ARM_R0
	ldmia   r14, {r0-r12}
	ldr     r13, [r14, #(MCONTEXT_ARM_SP - MCONTEXT_ARM_R0)]
	add     r14, r14, #(MCONTEXT_ARM_LR - MCONTEXT_ARM_R0)
	ldmia   r14, {r14, pc}
#elif defined(__x86_64__)
	.global xsetcontext
	.text
xsetcontext:	
#endif
