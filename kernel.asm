
kernel.elf:     file format elf64-x86-64


Disassembly of section .text:

ffffffff80000000 <kmain>:
ffffffff80000000:	f3 0f 1e fa          	endbr64
ffffffff80000004:	55                   	push   %rbp
ffffffff80000005:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000008:	e8 8d 00 00 00       	call   ffffffff8000009a <FbInit32>
ffffffff8000000d:	e8 d5 04 00 00       	call   ffffffff800004e7 <TerminalInit32>
ffffffff80000012:	bf 00 00 00 00       	mov    $0x0,%edi
ffffffff80000017:	e8 94 05 00 00       	call   ffffffff800005b0 <TerminalClear32>
ffffffff8000001c:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff80000021:	48 c7 c7 00 20 00 80 	mov    $0xffffffff80002000,%rdi
ffffffff80000028:	e8 4d 06 00 00       	call   ffffffff8000067a <TerminalPrintLine32>
ffffffff8000002d:	e8 f2 08 00 00       	call   ffffffff80000924 <ISRInitialize>
ffffffff80000032:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff80000037:	48 c7 c7 10 20 00 80 	mov    $0xffffffff80002010,%rdi
ffffffff8000003e:	e8 37 06 00 00       	call   ffffffff8000067a <TerminalPrintLine32>
ffffffff80000043:	e8 5c 09 00 00       	call   ffffffff800009a4 <IRQInitialize>
ffffffff80000048:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff8000004d:	48 c7 c7 17 20 00 80 	mov    $0xffffffff80002017,%rdi
ffffffff80000054:	e8 21 06 00 00       	call   ffffffff8000067a <TerminalPrintLine32>
ffffffff80000059:	e8 b3 0d 00 00       	call   ffffffff80000e11 <KbdInitialize>
ffffffff8000005e:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff80000063:	48 c7 c7 1e 20 00 80 	mov    $0xffffffff8000201e,%rdi
ffffffff8000006a:	e8 0b 06 00 00       	call   ffffffff8000067a <TerminalPrintLine32>
ffffffff8000006f:	e8 55 08 00 00       	call   ffffffff800008c9 <IDTInitialize>
ffffffff80000074:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff80000079:	48 c7 c7 25 20 00 80 	mov    $0xffffffff80002025,%rdi
ffffffff80000080:	e8 f5 05 00 00       	call   ffffffff8000067a <TerminalPrintLine32>
ffffffff80000085:	fb                   	sti
ffffffff80000086:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff8000008b:	48 c7 c7 2c 20 00 80 	mov    $0xffffffff8000202c,%rdi
ffffffff80000092:	e8 e3 05 00 00       	call   ffffffff8000067a <TerminalPrintLine32>
ffffffff80000097:	f4                   	hlt
ffffffff80000098:	eb fd                	jmp    ffffffff80000097 <kmain+0x97>

ffffffff8000009a <FbInit32>:
ffffffff8000009a:	f3 0f 1e fa          	endbr64
ffffffff8000009e:	55                   	push   %rbp
ffffffff8000009f:	48 89 e5             	mov    %rsp,%rbp
ffffffff800000a2:	48 8b 05 ff 2f 00 00 	mov    0x2fff(%rip),%rax        # ffffffff800030a8 <FrBuf32+0x28>
ffffffff800000a9:	48 85 c0             	test   %rax,%rax
ffffffff800000ac:	74 70                	je     ffffffff8000011e <FbInit32+0x84>
ffffffff800000ae:	48 8b 05 f3 2f 00 00 	mov    0x2ff3(%rip),%rax        # ffffffff800030a8 <FrBuf32+0x28>
ffffffff800000b5:	48 8b 40 10          	mov    0x10(%rax),%rax
ffffffff800000b9:	48 8b 00             	mov    (%rax),%rax
ffffffff800000bc:	48 89 05 3d 3f 00 00 	mov    %rax,0x3f3d(%rip)        # ffffffff80004000 <framebuffer>
ffffffff800000c3:	48 8b 05 36 3f 00 00 	mov    0x3f36(%rip),%rax        # ffffffff80004000 <framebuffer>
ffffffff800000ca:	48 8b 00             	mov    (%rax),%rax
ffffffff800000cd:	48 89 05 34 3f 00 00 	mov    %rax,0x3f34(%rip)        # ffffffff80004008 <fb_address>
ffffffff800000d4:	48 8b 05 25 3f 00 00 	mov    0x3f25(%rip),%rax        # ffffffff80004000 <framebuffer>
ffffffff800000db:	48 8b 40 08          	mov    0x8(%rax),%rax
ffffffff800000df:	48 89 05 2a 3f 00 00 	mov    %rax,0x3f2a(%rip)        # ffffffff80004010 <fb_width>
ffffffff800000e6:	48 8b 05 13 3f 00 00 	mov    0x3f13(%rip),%rax        # ffffffff80004000 <framebuffer>
ffffffff800000ed:	48 8b 40 10          	mov    0x10(%rax),%rax
ffffffff800000f1:	48 89 05 20 3f 00 00 	mov    %rax,0x3f20(%rip)        # ffffffff80004018 <fb_height>
ffffffff800000f8:	48 8b 05 01 3f 00 00 	mov    0x3f01(%rip),%rax        # ffffffff80004000 <framebuffer>
ffffffff800000ff:	48 8b 40 18          	mov    0x18(%rax),%rax
ffffffff80000103:	48 89 05 16 3f 00 00 	mov    %rax,0x3f16(%rip)        # ffffffff80004020 <fb_pitch>
ffffffff8000010a:	48 8b 05 ef 3e 00 00 	mov    0x3eef(%rip),%rax        # ffffffff80004000 <framebuffer>
ffffffff80000111:	0f b7 40 20          	movzwl 0x20(%rax),%eax
ffffffff80000115:	66 89 05 0c 3f 00 00 	mov    %ax,0x3f0c(%rip)        # ffffffff80004028 <fb_bpp>
ffffffff8000011c:	eb 01                	jmp    ffffffff8000011f <FbInit32+0x85>
ffffffff8000011e:	90                   	nop
ffffffff8000011f:	5d                   	pop    %rbp
ffffffff80000120:	c3                   	ret

ffffffff80000121 <FbPrintPixel32>:
ffffffff80000121:	f3 0f 1e fa          	endbr64
ffffffff80000125:	55                   	push   %rbp
ffffffff80000126:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000129:	48 83 ec 20          	sub    $0x20,%rsp
ffffffff8000012d:	89 7d ec             	mov    %edi,-0x14(%rbp)
ffffffff80000130:	89 75 e8             	mov    %esi,-0x18(%rbp)
ffffffff80000133:	89 55 e4             	mov    %edx,-0x1c(%rbp)
ffffffff80000136:	48 8b 0d cb 3e 00 00 	mov    0x3ecb(%rip),%rcx        # ffffffff80004008 <fb_address>
ffffffff8000013d:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff80000140:	48 8b 05 d9 3e 00 00 	mov    0x3ed9(%rip),%rax        # ffffffff80004020 <fb_pitch>
ffffffff80000147:	48 0f af c2          	imul   %rdx,%rax
ffffffff8000014b:	8b 55 ec             	mov    -0x14(%rbp),%edx
ffffffff8000014e:	c1 e2 02             	shl    $0x2,%edx
ffffffff80000151:	89 d2                	mov    %edx,%edx
ffffffff80000153:	48 01 d0             	add    %rdx,%rax
ffffffff80000156:	48 01 c8             	add    %rcx,%rax
ffffffff80000159:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
ffffffff8000015d:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff80000161:	8b 55 e4             	mov    -0x1c(%rbp),%edx
ffffffff80000164:	89 10                	mov    %edx,(%rax)
ffffffff80000166:	90                   	nop
ffffffff80000167:	c9                   	leave
ffffffff80000168:	c3                   	ret

ffffffff80000169 <FbCleanScreen32>:
ffffffff80000169:	f3 0f 1e fa          	endbr64
ffffffff8000016d:	55                   	push   %rbp
ffffffff8000016e:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000171:	48 83 ec 18          	sub    $0x18,%rsp
ffffffff80000175:	89 7d ec             	mov    %edi,-0x14(%rbp)
ffffffff80000178:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
ffffffff8000017f:	eb 32                	jmp    ffffffff800001b3 <FbCleanScreen32+0x4a>
ffffffff80000181:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%rbp)
ffffffff80000188:	eb 16                	jmp    ffffffff800001a0 <FbCleanScreen32+0x37>
ffffffff8000018a:	8b 55 ec             	mov    -0x14(%rbp),%edx
ffffffff8000018d:	8b 4d fc             	mov    -0x4(%rbp),%ecx
ffffffff80000190:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff80000193:	89 ce                	mov    %ecx,%esi
ffffffff80000195:	89 c7                	mov    %eax,%edi
ffffffff80000197:	e8 85 ff ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff8000019c:	83 45 f8 01          	addl   $0x1,-0x8(%rbp)
ffffffff800001a0:	8b 55 f8             	mov    -0x8(%rbp),%edx
ffffffff800001a3:	48 8b 05 66 3e 00 00 	mov    0x3e66(%rip),%rax        # ffffffff80004010 <fb_width>
ffffffff800001aa:	48 39 c2             	cmp    %rax,%rdx
ffffffff800001ad:	72 db                	jb     ffffffff8000018a <FbCleanScreen32+0x21>
ffffffff800001af:	83 45 fc 01          	addl   $0x1,-0x4(%rbp)
ffffffff800001b3:	8b 55 fc             	mov    -0x4(%rbp),%edx
ffffffff800001b6:	48 8b 05 5b 3e 00 00 	mov    0x3e5b(%rip),%rax        # ffffffff80004018 <fb_height>
ffffffff800001bd:	48 39 c2             	cmp    %rax,%rdx
ffffffff800001c0:	72 bf                	jb     ffffffff80000181 <FbCleanScreen32+0x18>
ffffffff800001c2:	90                   	nop
ffffffff800001c3:	90                   	nop
ffffffff800001c4:	c9                   	leave
ffffffff800001c5:	c3                   	ret

ffffffff800001c6 <FbDrawChar32>:
ffffffff800001c6:	f3 0f 1e fa          	endbr64
ffffffff800001ca:	55                   	push   %rbp
ffffffff800001cb:	48 89 e5             	mov    %rsp,%rbp
ffffffff800001ce:	48 83 ec 20          	sub    $0x20,%rsp
ffffffff800001d2:	89 f8                	mov    %edi,%eax
ffffffff800001d4:	89 75 e8             	mov    %esi,-0x18(%rbp)
ffffffff800001d7:	89 55 e4             	mov    %edx,-0x1c(%rbp)
ffffffff800001da:	89 4d e0             	mov    %ecx,-0x20(%rbp)
ffffffff800001dd:	88 45 ec             	mov    %al,-0x14(%rbp)
ffffffff800001e0:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
ffffffff800001e7:	eb 5e                	jmp    ffffffff80000247 <FbDrawChar32+0x81>
ffffffff800001e9:	0f b6 55 ec          	movzbl -0x14(%rbp),%edx
ffffffff800001ed:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff800001f0:	48 63 d2             	movslq %edx,%rdx
ffffffff800001f3:	0f b6 84 d0 60 20 00 	movzbl -0x7fffdfa0(%rax,%rdx,8),%eax
ffffffff800001fa:	80 
ffffffff800001fb:	88 45 f7             	mov    %al,-0x9(%rbp)
ffffffff800001fe:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%rbp)
ffffffff80000205:	eb 36                	jmp    ffffffff8000023d <FbDrawChar32+0x77>
ffffffff80000207:	0f b6 55 f7          	movzbl -0x9(%rbp),%edx
ffffffff8000020b:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff8000020e:	89 c1                	mov    %eax,%ecx
ffffffff80000210:	d3 fa                	sar    %cl,%edx
ffffffff80000212:	89 d0                	mov    %edx,%eax
ffffffff80000214:	83 e0 01             	and    $0x1,%eax
ffffffff80000217:	85 c0                	test   %eax,%eax
ffffffff80000219:	74 1e                	je     ffffffff80000239 <FbDrawChar32+0x73>
ffffffff8000021b:	8b 55 e4             	mov    -0x1c(%rbp),%edx
ffffffff8000021e:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff80000221:	8d 34 02             	lea    (%rdx,%rax,1),%esi
ffffffff80000224:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff80000227:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff8000022a:	8d 0c 02             	lea    (%rdx,%rax,1),%ecx
ffffffff8000022d:	8b 45 e0             	mov    -0x20(%rbp),%eax
ffffffff80000230:	89 c2                	mov    %eax,%edx
ffffffff80000232:	89 cf                	mov    %ecx,%edi
ffffffff80000234:	e8 e8 fe ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff80000239:	83 45 f8 01          	addl   $0x1,-0x8(%rbp)
ffffffff8000023d:	83 7d f8 07          	cmpl   $0x7,-0x8(%rbp)
ffffffff80000241:	76 c4                	jbe    ffffffff80000207 <FbDrawChar32+0x41>
ffffffff80000243:	83 45 fc 01          	addl   $0x1,-0x4(%rbp)
ffffffff80000247:	83 7d fc 07          	cmpl   $0x7,-0x4(%rbp)
ffffffff8000024b:	76 9c                	jbe    ffffffff800001e9 <FbDrawChar32+0x23>
ffffffff8000024d:	90                   	nop
ffffffff8000024e:	90                   	nop
ffffffff8000024f:	c9                   	leave
ffffffff80000250:	c3                   	ret

ffffffff80000251 <FbPrintString32>:
ffffffff80000251:	f3 0f 1e fa          	endbr64
ffffffff80000255:	55                   	push   %rbp
ffffffff80000256:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000259:	48 83 ec 18          	sub    $0x18,%rsp
ffffffff8000025d:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
ffffffff80000261:	89 75 f4             	mov    %esi,-0xc(%rbp)
ffffffff80000264:	89 55 f0             	mov    %edx,-0x10(%rbp)
ffffffff80000267:	89 4d ec             	mov    %ecx,-0x14(%rbp)
ffffffff8000026a:	eb 5f                	jmp    ffffffff800002cb <FbPrintString32+0x7a>
ffffffff8000026c:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff80000270:	0f b6 00             	movzbl (%rax),%eax
ffffffff80000273:	3c 0a                	cmp    $0xa,%al
ffffffff80000275:	75 12                	jne    ffffffff80000289 <FbPrintString32+0x38>
ffffffff80000277:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%rbp)
ffffffff8000027e:	83 45 f0 08          	addl   $0x8,-0x10(%rbp)
ffffffff80000282:	48 83 45 f8 01       	addq   $0x1,-0x8(%rbp)
ffffffff80000287:	eb 42                	jmp    ffffffff800002cb <FbPrintString32+0x7a>
ffffffff80000289:	8b 45 f4             	mov    -0xc(%rbp),%eax
ffffffff8000028c:	83 c0 08             	add    $0x8,%eax
ffffffff8000028f:	89 c2                	mov    %eax,%edx
ffffffff80000291:	48 8b 05 78 3d 00 00 	mov    0x3d78(%rip),%rax        # ffffffff80004010 <fb_width>
ffffffff80000298:	48 39 d0             	cmp    %rdx,%rax
ffffffff8000029b:	73 0b                	jae    ffffffff800002a8 <FbPrintString32+0x57>
ffffffff8000029d:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%rbp)
ffffffff800002a4:	83 45 f0 08          	addl   $0x8,-0x10(%rbp)
ffffffff800002a8:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff800002ac:	0f b6 00             	movzbl (%rax),%eax
ffffffff800002af:	0f b6 c0             	movzbl %al,%eax
ffffffff800002b2:	8b 4d ec             	mov    -0x14(%rbp),%ecx
ffffffff800002b5:	8b 55 f0             	mov    -0x10(%rbp),%edx
ffffffff800002b8:	8b 75 f4             	mov    -0xc(%rbp),%esi
ffffffff800002bb:	89 c7                	mov    %eax,%edi
ffffffff800002bd:	e8 04 ff ff ff       	call   ffffffff800001c6 <FbDrawChar32>
ffffffff800002c2:	48 83 45 f8 01       	addq   $0x1,-0x8(%rbp)
ffffffff800002c7:	83 45 f4 08          	addl   $0x8,-0xc(%rbp)
ffffffff800002cb:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff800002cf:	0f b6 00             	movzbl (%rax),%eax
ffffffff800002d2:	84 c0                	test   %al,%al
ffffffff800002d4:	75 96                	jne    ffffffff8000026c <FbPrintString32+0x1b>
ffffffff800002d6:	90                   	nop
ffffffff800002d7:	90                   	nop
ffffffff800002d8:	c9                   	leave
ffffffff800002d9:	c3                   	ret

ffffffff800002da <FbFillRect32>:
ffffffff800002da:	f3 0f 1e fa          	endbr64
ffffffff800002de:	55                   	push   %rbp
ffffffff800002df:	48 89 e5             	mov    %rsp,%rbp
ffffffff800002e2:	48 83 ec 28          	sub    $0x28,%rsp
ffffffff800002e6:	89 7d ec             	mov    %edi,-0x14(%rbp)
ffffffff800002e9:	89 75 e8             	mov    %esi,-0x18(%rbp)
ffffffff800002ec:	89 55 e4             	mov    %edx,-0x1c(%rbp)
ffffffff800002ef:	89 4d e0             	mov    %ecx,-0x20(%rbp)
ffffffff800002f2:	44 89 45 dc          	mov    %r8d,-0x24(%rbp)
ffffffff800002f6:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
ffffffff800002fd:	eb 37                	jmp    ffffffff80000336 <FbFillRect32+0x5c>
ffffffff800002ff:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%rbp)
ffffffff80000306:	eb 22                	jmp    ffffffff8000032a <FbFillRect32+0x50>
ffffffff80000308:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff8000030b:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff8000030e:	8d 34 02             	lea    (%rdx,%rax,1),%esi
ffffffff80000311:	8b 55 ec             	mov    -0x14(%rbp),%edx
ffffffff80000314:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff80000317:	8d 0c 02             	lea    (%rdx,%rax,1),%ecx
ffffffff8000031a:	8b 45 dc             	mov    -0x24(%rbp),%eax
ffffffff8000031d:	89 c2                	mov    %eax,%edx
ffffffff8000031f:	89 cf                	mov    %ecx,%edi
ffffffff80000321:	e8 fb fd ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff80000326:	83 45 f8 01          	addl   $0x1,-0x8(%rbp)
ffffffff8000032a:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff8000032d:	3b 45 e4             	cmp    -0x1c(%rbp),%eax
ffffffff80000330:	72 d6                	jb     ffffffff80000308 <FbFillRect32+0x2e>
ffffffff80000332:	83 45 fc 01          	addl   $0x1,-0x4(%rbp)
ffffffff80000336:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff80000339:	3b 45 e0             	cmp    -0x20(%rbp),%eax
ffffffff8000033c:	72 c1                	jb     ffffffff800002ff <FbFillRect32+0x25>
ffffffff8000033e:	90                   	nop
ffffffff8000033f:	90                   	nop
ffffffff80000340:	c9                   	leave
ffffffff80000341:	c3                   	ret

ffffffff80000342 <FbGetWidth32>:
ffffffff80000342:	f3 0f 1e fa          	endbr64
ffffffff80000346:	55                   	push   %rbp
ffffffff80000347:	48 89 e5             	mov    %rsp,%rbp
ffffffff8000034a:	48 8b 05 bf 3c 00 00 	mov    0x3cbf(%rip),%rax        # ffffffff80004010 <fb_width>
ffffffff80000351:	5d                   	pop    %rbp
ffffffff80000352:	c3                   	ret

ffffffff80000353 <FbGetHeight32>:
ffffffff80000353:	f3 0f 1e fa          	endbr64
ffffffff80000357:	55                   	push   %rbp
ffffffff80000358:	48 89 e5             	mov    %rsp,%rbp
ffffffff8000035b:	48 8b 05 b6 3c 00 00 	mov    0x3cb6(%rip),%rax        # ffffffff80004018 <fb_height>
ffffffff80000362:	5d                   	pop    %rbp
ffffffff80000363:	c3                   	ret

ffffffff80000364 <FbDrawRect32>:
ffffffff80000364:	f3 0f 1e fa          	endbr64
ffffffff80000368:	55                   	push   %rbp
ffffffff80000369:	48 89 e5             	mov    %rsp,%rbp
ffffffff8000036c:	48 83 ec 28          	sub    $0x28,%rsp
ffffffff80000370:	89 7d ec             	mov    %edi,-0x14(%rbp)
ffffffff80000373:	89 75 e8             	mov    %esi,-0x18(%rbp)
ffffffff80000376:	89 55 e4             	mov    %edx,-0x1c(%rbp)
ffffffff80000379:	89 4d e0             	mov    %ecx,-0x20(%rbp)
ffffffff8000037c:	44 89 45 dc          	mov    %r8d,-0x24(%rbp)
ffffffff80000380:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
ffffffff80000387:	eb 3c                	jmp    ffffffff800003c5 <FbDrawRect32+0x61>
ffffffff80000389:	8b 55 ec             	mov    -0x14(%rbp),%edx
ffffffff8000038c:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff8000038f:	8d 0c 02             	lea    (%rdx,%rax,1),%ecx
ffffffff80000392:	8b 55 dc             	mov    -0x24(%rbp),%edx
ffffffff80000395:	8b 45 e8             	mov    -0x18(%rbp),%eax
ffffffff80000398:	89 c6                	mov    %eax,%esi
ffffffff8000039a:	89 cf                	mov    %ecx,%edi
ffffffff8000039c:	e8 80 fd ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff800003a1:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff800003a4:	8b 45 e0             	mov    -0x20(%rbp),%eax
ffffffff800003a7:	01 d0                	add    %edx,%eax
ffffffff800003a9:	8d 70 ff             	lea    -0x1(%rax),%esi
ffffffff800003ac:	8b 55 ec             	mov    -0x14(%rbp),%edx
ffffffff800003af:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff800003b2:	8d 0c 02             	lea    (%rdx,%rax,1),%ecx
ffffffff800003b5:	8b 45 dc             	mov    -0x24(%rbp),%eax
ffffffff800003b8:	89 c2                	mov    %eax,%edx
ffffffff800003ba:	89 cf                	mov    %ecx,%edi
ffffffff800003bc:	e8 60 fd ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff800003c1:	83 45 fc 01          	addl   $0x1,-0x4(%rbp)
ffffffff800003c5:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff800003c8:	3b 45 e4             	cmp    -0x1c(%rbp),%eax
ffffffff800003cb:	72 bc                	jb     ffffffff80000389 <FbDrawRect32+0x25>
ffffffff800003cd:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%rbp)
ffffffff800003d4:	eb 3c                	jmp    ffffffff80000412 <FbDrawRect32+0xae>
ffffffff800003d6:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff800003d9:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff800003dc:	8d 0c 02             	lea    (%rdx,%rax,1),%ecx
ffffffff800003df:	8b 55 dc             	mov    -0x24(%rbp),%edx
ffffffff800003e2:	8b 45 ec             	mov    -0x14(%rbp),%eax
ffffffff800003e5:	89 ce                	mov    %ecx,%esi
ffffffff800003e7:	89 c7                	mov    %eax,%edi
ffffffff800003e9:	e8 33 fd ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff800003ee:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff800003f1:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff800003f4:	8d 34 02             	lea    (%rdx,%rax,1),%esi
ffffffff800003f7:	8b 55 ec             	mov    -0x14(%rbp),%edx
ffffffff800003fa:	8b 45 e4             	mov    -0x1c(%rbp),%eax
ffffffff800003fd:	01 d0                	add    %edx,%eax
ffffffff800003ff:	8d 48 ff             	lea    -0x1(%rax),%ecx
ffffffff80000402:	8b 45 dc             	mov    -0x24(%rbp),%eax
ffffffff80000405:	89 c2                	mov    %eax,%edx
ffffffff80000407:	89 cf                	mov    %ecx,%edi
ffffffff80000409:	e8 13 fd ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff8000040e:	83 45 f8 01          	addl   $0x1,-0x8(%rbp)
ffffffff80000412:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff80000415:	3b 45 e0             	cmp    -0x20(%rbp),%eax
ffffffff80000418:	72 bc                	jb     ffffffff800003d6 <FbDrawRect32+0x72>
ffffffff8000041a:	90                   	nop
ffffffff8000041b:	90                   	nop
ffffffff8000041c:	c9                   	leave
ffffffff8000041d:	c3                   	ret

ffffffff8000041e <FbReadPixel32>:
ffffffff8000041e:	f3 0f 1e fa          	endbr64
ffffffff80000422:	55                   	push   %rbp
ffffffff80000423:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000426:	48 83 ec 18          	sub    $0x18,%rsp
ffffffff8000042a:	89 7d ec             	mov    %edi,-0x14(%rbp)
ffffffff8000042d:	89 75 e8             	mov    %esi,-0x18(%rbp)
ffffffff80000430:	48 8b 0d d1 3b 00 00 	mov    0x3bd1(%rip),%rcx        # ffffffff80004008 <fb_address>
ffffffff80000437:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff8000043a:	48 8b 05 df 3b 00 00 	mov    0x3bdf(%rip),%rax        # ffffffff80004020 <fb_pitch>
ffffffff80000441:	48 0f af c2          	imul   %rdx,%rax
ffffffff80000445:	8b 55 ec             	mov    -0x14(%rbp),%edx
ffffffff80000448:	c1 e2 02             	shl    $0x2,%edx
ffffffff8000044b:	89 d2                	mov    %edx,%edx
ffffffff8000044d:	48 01 d0             	add    %rdx,%rax
ffffffff80000450:	48 01 c8             	add    %rcx,%rax
ffffffff80000453:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
ffffffff80000457:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff8000045b:	8b 00                	mov    (%rax),%eax
ffffffff8000045d:	c9                   	leave
ffffffff8000045e:	c3                   	ret

ffffffff8000045f <FbCopyRect32>:
ffffffff8000045f:	f3 0f 1e fa          	endbr64
ffffffff80000463:	55                   	push   %rbp
ffffffff80000464:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000467:	48 83 ec 28          	sub    $0x28,%rsp
ffffffff8000046b:	89 7d ec             	mov    %edi,-0x14(%rbp)
ffffffff8000046e:	89 75 e8             	mov    %esi,-0x18(%rbp)
ffffffff80000471:	89 55 e4             	mov    %edx,-0x1c(%rbp)
ffffffff80000474:	89 4d e0             	mov    %ecx,-0x20(%rbp)
ffffffff80000477:	44 89 45 dc          	mov    %r8d,-0x24(%rbp)
ffffffff8000047b:	44 89 4d d8          	mov    %r9d,-0x28(%rbp)
ffffffff8000047f:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
ffffffff80000486:	eb 53                	jmp    ffffffff800004db <FbCopyRect32+0x7c>
ffffffff80000488:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%rbp)
ffffffff8000048f:	eb 3e                	jmp    ffffffff800004cf <FbCopyRect32+0x70>
ffffffff80000491:	8b 55 e8             	mov    -0x18(%rbp),%edx
ffffffff80000494:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff80000497:	01 c2                	add    %eax,%edx
ffffffff80000499:	8b 4d ec             	mov    -0x14(%rbp),%ecx
ffffffff8000049c:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff8000049f:	01 c8                	add    %ecx,%eax
ffffffff800004a1:	89 d6                	mov    %edx,%esi
ffffffff800004a3:	89 c7                	mov    %eax,%edi
ffffffff800004a5:	e8 74 ff ff ff       	call   ffffffff8000041e <FbReadPixel32>
ffffffff800004aa:	89 45 f4             	mov    %eax,-0xc(%rbp)
ffffffff800004ad:	8b 55 e0             	mov    -0x20(%rbp),%edx
ffffffff800004b0:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff800004b3:	8d 34 02             	lea    (%rdx,%rax,1),%esi
ffffffff800004b6:	8b 55 e4             	mov    -0x1c(%rbp),%edx
ffffffff800004b9:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff800004bc:	8d 0c 02             	lea    (%rdx,%rax,1),%ecx
ffffffff800004bf:	8b 45 f4             	mov    -0xc(%rbp),%eax
ffffffff800004c2:	89 c2                	mov    %eax,%edx
ffffffff800004c4:	89 cf                	mov    %ecx,%edi
ffffffff800004c6:	e8 56 fc ff ff       	call   ffffffff80000121 <FbPrintPixel32>
ffffffff800004cb:	83 45 f8 01          	addl   $0x1,-0x8(%rbp)
ffffffff800004cf:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff800004d2:	3b 45 dc             	cmp    -0x24(%rbp),%eax
ffffffff800004d5:	72 ba                	jb     ffffffff80000491 <FbCopyRect32+0x32>
ffffffff800004d7:	83 45 fc 01          	addl   $0x1,-0x4(%rbp)
ffffffff800004db:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff800004de:	3b 45 d8             	cmp    -0x28(%rbp),%eax
ffffffff800004e1:	72 a5                	jb     ffffffff80000488 <FbCopyRect32+0x29>
ffffffff800004e3:	90                   	nop
ffffffff800004e4:	90                   	nop
ffffffff800004e5:	c9                   	leave
ffffffff800004e6:	c3                   	ret

ffffffff800004e7 <TerminalInit32>:
ffffffff800004e7:	f3 0f 1e fa          	endbr64
ffffffff800004eb:	55                   	push   %rbp
ffffffff800004ec:	48 89 e5             	mov    %rsp,%rbp
ffffffff800004ef:	c7 05 33 3b 00 00 00 	movl   $0x0,0x3b33(%rip)        # ffffffff8000402c <CursX>
ffffffff800004f6:	00 00 00 
ffffffff800004f9:	c7 05 2d 3b 00 00 00 	movl   $0x0,0x3b2d(%rip)        # ffffffff80004030 <CursY>
ffffffff80000500:	00 00 00 
ffffffff80000503:	90                   	nop
ffffffff80000504:	5d                   	pop    %rbp
ffffffff80000505:	c3                   	ret

ffffffff80000506 <TerminalPutChar32>:
ffffffff80000506:	f3 0f 1e fa          	endbr64
ffffffff8000050a:	55                   	push   %rbp
ffffffff8000050b:	48 89 e5             	mov    %rsp,%rbp
ffffffff8000050e:	53                   	push   %rbx
ffffffff8000050f:	48 83 ec 18          	sub    $0x18,%rsp
ffffffff80000513:	89 f8                	mov    %edi,%eax
ffffffff80000515:	89 75 e8             	mov    %esi,-0x18(%rbp)
ffffffff80000518:	88 45 ec             	mov    %al,-0x14(%rbp)
ffffffff8000051b:	80 7d ec 0a          	cmpb   $0xa,-0x14(%rbp)
ffffffff8000051f:	75 07                	jne    ffffffff80000528 <TerminalPutChar32+0x22>
ffffffff80000521:	e8 e1 00 00 00       	call   ffffffff80000607 <TerminalNewLine32>
ffffffff80000526:	eb 43                	jmp    ffffffff8000056b <TerminalPutChar32+0x65>
ffffffff80000528:	8b 15 02 3b 00 00    	mov    0x3b02(%rip),%edx        # ffffffff80004030 <CursY>
ffffffff8000052e:	8b 35 f8 3a 00 00    	mov    0x3af8(%rip),%esi        # ffffffff8000402c <CursX>
ffffffff80000534:	0f b6 45 ec          	movzbl -0x14(%rbp),%eax
ffffffff80000538:	0f b6 c0             	movzbl %al,%eax
ffffffff8000053b:	8b 4d e8             	mov    -0x18(%rbp),%ecx
ffffffff8000053e:	89 c7                	mov    %eax,%edi
ffffffff80000540:	e8 81 fc ff ff       	call   ffffffff800001c6 <FbDrawChar32>
ffffffff80000545:	8b 05 e1 3a 00 00    	mov    0x3ae1(%rip),%eax        # ffffffff8000402c <CursX>
ffffffff8000054b:	83 c0 08             	add    $0x8,%eax
ffffffff8000054e:	89 05 d8 3a 00 00    	mov    %eax,0x3ad8(%rip)        # ffffffff8000402c <CursX>
ffffffff80000554:	8b 05 d2 3a 00 00    	mov    0x3ad2(%rip),%eax        # ffffffff8000402c <CursX>
ffffffff8000055a:	8d 58 08             	lea    0x8(%rax),%ebx
ffffffff8000055d:	e8 e0 fd ff ff       	call   ffffffff80000342 <FbGetWidth32>
ffffffff80000562:	39 d8                	cmp    %ebx,%eax
ffffffff80000564:	73 05                	jae    ffffffff8000056b <TerminalPutChar32+0x65>
ffffffff80000566:	e8 9c 00 00 00       	call   ffffffff80000607 <TerminalNewLine32>
ffffffff8000056b:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
ffffffff8000056f:	c9                   	leave
ffffffff80000570:	c3                   	ret

ffffffff80000571 <TerminalWrite32>:
ffffffff80000571:	f3 0f 1e fa          	endbr64
ffffffff80000575:	55                   	push   %rbp
ffffffff80000576:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000579:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff8000057d:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
ffffffff80000581:	89 75 f4             	mov    %esi,-0xc(%rbp)
ffffffff80000584:	eb 1b                	jmp    ffffffff800005a1 <TerminalWrite32+0x30>
ffffffff80000586:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff8000058a:	0f b6 00             	movzbl (%rax),%eax
ffffffff8000058d:	0f be c0             	movsbl %al,%eax
ffffffff80000590:	8b 55 f4             	mov    -0xc(%rbp),%edx
ffffffff80000593:	89 d6                	mov    %edx,%esi
ffffffff80000595:	89 c7                	mov    %eax,%edi
ffffffff80000597:	e8 6a ff ff ff       	call   ffffffff80000506 <TerminalPutChar32>
ffffffff8000059c:	48 83 45 f8 01       	addq   $0x1,-0x8(%rbp)
ffffffff800005a1:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff800005a5:	0f b6 00             	movzbl (%rax),%eax
ffffffff800005a8:	84 c0                	test   %al,%al
ffffffff800005aa:	75 da                	jne    ffffffff80000586 <TerminalWrite32+0x15>
ffffffff800005ac:	90                   	nop
ffffffff800005ad:	90                   	nop
ffffffff800005ae:	c9                   	leave
ffffffff800005af:	c3                   	ret

ffffffff800005b0 <TerminalClear32>:
ffffffff800005b0:	f3 0f 1e fa          	endbr64
ffffffff800005b4:	55                   	push   %rbp
ffffffff800005b5:	48 89 e5             	mov    %rsp,%rbp
ffffffff800005b8:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff800005bc:	89 7d fc             	mov    %edi,-0x4(%rbp)
ffffffff800005bf:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff800005c2:	89 c7                	mov    %eax,%edi
ffffffff800005c4:	e8 a0 fb ff ff       	call   ffffffff80000169 <FbCleanScreen32>
ffffffff800005c9:	c7 05 59 3a 00 00 00 	movl   $0x0,0x3a59(%rip)        # ffffffff8000402c <CursX>
ffffffff800005d0:	00 00 00 
ffffffff800005d3:	c7 05 53 3a 00 00 00 	movl   $0x0,0x3a53(%rip)        # ffffffff80004030 <CursY>
ffffffff800005da:	00 00 00 
ffffffff800005dd:	90                   	nop
ffffffff800005de:	c9                   	leave
ffffffff800005df:	c3                   	ret

ffffffff800005e0 <TerminalSetCursor32>:
ffffffff800005e0:	f3 0f 1e fa          	endbr64
ffffffff800005e4:	55                   	push   %rbp
ffffffff800005e5:	48 89 e5             	mov    %rsp,%rbp
ffffffff800005e8:	48 83 ec 08          	sub    $0x8,%rsp
ffffffff800005ec:	89 7d fc             	mov    %edi,-0x4(%rbp)
ffffffff800005ef:	89 75 f8             	mov    %esi,-0x8(%rbp)
ffffffff800005f2:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff800005f5:	89 05 31 3a 00 00    	mov    %eax,0x3a31(%rip)        # ffffffff8000402c <CursX>
ffffffff800005fb:	8b 45 f8             	mov    -0x8(%rbp),%eax
ffffffff800005fe:	89 05 2c 3a 00 00    	mov    %eax,0x3a2c(%rip)        # ffffffff80004030 <CursY>
ffffffff80000604:	90                   	nop
ffffffff80000605:	c9                   	leave
ffffffff80000606:	c3                   	ret

ffffffff80000607 <TerminalNewLine32>:
ffffffff80000607:	f3 0f 1e fa          	endbr64
ffffffff8000060b:	55                   	push   %rbp
ffffffff8000060c:	48 89 e5             	mov    %rsp,%rbp
ffffffff8000060f:	53                   	push   %rbx
ffffffff80000610:	48 83 ec 08          	sub    $0x8,%rsp
ffffffff80000614:	c7 05 0e 3a 00 00 00 	movl   $0x0,0x3a0e(%rip)        # ffffffff8000402c <CursX>
ffffffff8000061b:	00 00 00 
ffffffff8000061e:	8b 05 0c 3a 00 00    	mov    0x3a0c(%rip),%eax        # ffffffff80004030 <CursY>
ffffffff80000624:	83 c0 08             	add    $0x8,%eax
ffffffff80000627:	89 05 03 3a 00 00    	mov    %eax,0x3a03(%rip)        # ffffffff80004030 <CursY>
ffffffff8000062d:	8b 05 fd 39 00 00    	mov    0x39fd(%rip),%eax        # ffffffff80004030 <CursY>
ffffffff80000633:	8d 58 08             	lea    0x8(%rax),%ebx
ffffffff80000636:	e8 18 fd ff ff       	call   ffffffff80000353 <FbGetHeight32>
ffffffff8000063b:	39 d8                	cmp    %ebx,%eax
ffffffff8000063d:	73 05                	jae    ffffffff80000644 <TerminalNewLine32+0x3d>
ffffffff8000063f:	e8 ab 00 00 00       	call   ffffffff800006ef <TerminalScroll32>
ffffffff80000644:	90                   	nop
ffffffff80000645:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
ffffffff80000649:	c9                   	leave
ffffffff8000064a:	c3                   	ret

ffffffff8000064b <TerminalGetCursor32>:
ffffffff8000064b:	f3 0f 1e fa          	endbr64
ffffffff8000064f:	55                   	push   %rbp
ffffffff80000650:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000653:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000657:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
ffffffff8000065b:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
ffffffff8000065f:	8b 15 c7 39 00 00    	mov    0x39c7(%rip),%edx        # ffffffff8000402c <CursX>
ffffffff80000665:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff80000669:	89 10                	mov    %edx,(%rax)
ffffffff8000066b:	8b 15 bf 39 00 00    	mov    0x39bf(%rip),%edx        # ffffffff80004030 <CursY>
ffffffff80000671:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
ffffffff80000675:	89 10                	mov    %edx,(%rax)
ffffffff80000677:	90                   	nop
ffffffff80000678:	c9                   	leave
ffffffff80000679:	c3                   	ret

ffffffff8000067a <TerminalPrintLine32>:
ffffffff8000067a:	f3 0f 1e fa          	endbr64
ffffffff8000067e:	55                   	push   %rbp
ffffffff8000067f:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000682:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000686:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
ffffffff8000068a:	89 75 f4             	mov    %esi,-0xc(%rbp)
ffffffff8000068d:	8b 55 f4             	mov    -0xc(%rbp),%edx
ffffffff80000690:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff80000694:	89 d6                	mov    %edx,%esi
ffffffff80000696:	48 89 c7             	mov    %rax,%rdi
ffffffff80000699:	e8 d3 fe ff ff       	call   ffffffff80000571 <TerminalWrite32>
ffffffff8000069e:	e8 64 ff ff ff       	call   ffffffff80000607 <TerminalNewLine32>
ffffffff800006a3:	90                   	nop
ffffffff800006a4:	c9                   	leave
ffffffff800006a5:	c3                   	ret

ffffffff800006a6 <TerminalBackspace32>:
ffffffff800006a6:	f3 0f 1e fa          	endbr64
ffffffff800006aa:	55                   	push   %rbp
ffffffff800006ab:	48 89 e5             	mov    %rsp,%rbp
ffffffff800006ae:	8b 05 78 39 00 00    	mov    0x3978(%rip),%eax        # ffffffff8000402c <CursX>
ffffffff800006b4:	85 c0                	test   %eax,%eax
ffffffff800006b6:	74 34                	je     ffffffff800006ec <TerminalBackspace32+0x46>
ffffffff800006b8:	8b 05 6e 39 00 00    	mov    0x396e(%rip),%eax        # ffffffff8000402c <CursX>
ffffffff800006be:	83 e8 08             	sub    $0x8,%eax
ffffffff800006c1:	89 05 65 39 00 00    	mov    %eax,0x3965(%rip)        # ffffffff8000402c <CursX>
ffffffff800006c7:	8b 35 63 39 00 00    	mov    0x3963(%rip),%esi        # ffffffff80004030 <CursY>
ffffffff800006cd:	8b 05 59 39 00 00    	mov    0x3959(%rip),%eax        # ffffffff8000402c <CursX>
ffffffff800006d3:	41 b8 00 00 00 00    	mov    $0x0,%r8d
ffffffff800006d9:	b9 08 00 00 00       	mov    $0x8,%ecx
ffffffff800006de:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff800006e3:	89 c7                	mov    %eax,%edi
ffffffff800006e5:	e8 f0 fb ff ff       	call   ffffffff800002da <FbFillRect32>
ffffffff800006ea:	eb 01                	jmp    ffffffff800006ed <TerminalBackspace32+0x47>
ffffffff800006ec:	90                   	nop
ffffffff800006ed:	5d                   	pop    %rbp
ffffffff800006ee:	c3                   	ret

ffffffff800006ef <TerminalScroll32>:
ffffffff800006ef:	f3 0f 1e fa          	endbr64
ffffffff800006f3:	55                   	push   %rbp
ffffffff800006f4:	48 89 e5             	mov    %rsp,%rbp
ffffffff800006f7:	53                   	push   %rbx
ffffffff800006f8:	48 83 ec 08          	sub    $0x8,%rsp
ffffffff800006fc:	e8 52 fc ff ff       	call   ffffffff80000353 <FbGetHeight32>
ffffffff80000701:	8d 58 f8             	lea    -0x8(%rax),%ebx
ffffffff80000704:	e8 39 fc ff ff       	call   ffffffff80000342 <FbGetWidth32>
ffffffff80000709:	41 89 d9             	mov    %ebx,%r9d
ffffffff8000070c:	41 89 c0             	mov    %eax,%r8d
ffffffff8000070f:	b9 00 00 00 00       	mov    $0x0,%ecx
ffffffff80000714:	ba 00 00 00 00       	mov    $0x0,%edx
ffffffff80000719:	be 08 00 00 00       	mov    $0x8,%esi
ffffffff8000071e:	bf 00 00 00 00       	mov    $0x0,%edi
ffffffff80000723:	e8 37 fd ff ff       	call   ffffffff8000045f <FbCopyRect32>
ffffffff80000728:	e8 15 fc ff ff       	call   ffffffff80000342 <FbGetWidth32>
ffffffff8000072d:	89 c3                	mov    %eax,%ebx
ffffffff8000072f:	e8 1f fc ff ff       	call   ffffffff80000353 <FbGetHeight32>
ffffffff80000734:	83 e8 08             	sub    $0x8,%eax
ffffffff80000737:	41 b8 00 00 00 00    	mov    $0x0,%r8d
ffffffff8000073d:	b9 08 00 00 00       	mov    $0x8,%ecx
ffffffff80000742:	89 da                	mov    %ebx,%edx
ffffffff80000744:	89 c6                	mov    %eax,%esi
ffffffff80000746:	bf 00 00 00 00       	mov    $0x0,%edi
ffffffff8000074b:	e8 8a fb ff ff       	call   ffffffff800002da <FbFillRect32>
ffffffff80000750:	c7 05 d2 38 00 00 00 	movl   $0x0,0x38d2(%rip)        # ffffffff8000402c <CursX>
ffffffff80000757:	00 00 00 
ffffffff8000075a:	e8 f4 fb ff ff       	call   ffffffff80000353 <FbGetHeight32>
ffffffff8000075f:	83 e8 08             	sub    $0x8,%eax
ffffffff80000762:	89 05 c8 38 00 00    	mov    %eax,0x38c8(%rip)        # ffffffff80004030 <CursY>
ffffffff80000768:	90                   	nop
ffffffff80000769:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
ffffffff8000076d:	c9                   	leave
ffffffff8000076e:	c3                   	ret

ffffffff8000076f <TerminalPrintHex64>:
ffffffff8000076f:	f3 0f 1e fa          	endbr64
ffffffff80000773:	55                   	push   %rbp
ffffffff80000774:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000777:	48 83 ec 20          	sub    $0x20,%rsp
ffffffff8000077b:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
ffffffff8000077f:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff80000784:	48 c7 c7 3f 20 00 80 	mov    $0xffffffff8000203f,%rdi
ffffffff8000078b:	e8 e1 fd ff ff       	call   ffffffff80000571 <TerminalWrite32>
ffffffff80000790:	c7 45 fc 3c 00 00 00 	movl   $0x3c,-0x4(%rbp)
ffffffff80000797:	eb 4d                	jmp    ffffffff800007e6 <TerminalPrintHex64+0x77>
ffffffff80000799:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff8000079c:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
ffffffff800007a0:	89 c1                	mov    %eax,%ecx
ffffffff800007a2:	48 d3 ea             	shr    %cl,%rdx
ffffffff800007a5:	48 89 d0             	mov    %rdx,%rax
ffffffff800007a8:	83 e0 0f             	and    $0xf,%eax
ffffffff800007ab:	88 45 fb             	mov    %al,-0x5(%rbp)
ffffffff800007ae:	80 7d fb 09          	cmpb   $0x9,-0x5(%rbp)
ffffffff800007b2:	77 18                	ja     ffffffff800007cc <TerminalPrintHex64+0x5d>
ffffffff800007b4:	0f b6 45 fb          	movzbl -0x5(%rbp),%eax
ffffffff800007b8:	83 c0 30             	add    $0x30,%eax
ffffffff800007bb:	0f be c0             	movsbl %al,%eax
ffffffff800007be:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff800007c3:	89 c7                	mov    %eax,%edi
ffffffff800007c5:	e8 3c fd ff ff       	call   ffffffff80000506 <TerminalPutChar32>
ffffffff800007ca:	eb 16                	jmp    ffffffff800007e2 <TerminalPrintHex64+0x73>
ffffffff800007cc:	0f b6 45 fb          	movzbl -0x5(%rbp),%eax
ffffffff800007d0:	83 c0 37             	add    $0x37,%eax
ffffffff800007d3:	0f be c0             	movsbl %al,%eax
ffffffff800007d6:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff800007db:	89 c7                	mov    %eax,%edi
ffffffff800007dd:	e8 24 fd ff ff       	call   ffffffff80000506 <TerminalPutChar32>
ffffffff800007e2:	83 6d fc 04          	subl   $0x4,-0x4(%rbp)
ffffffff800007e6:	83 7d fc 00          	cmpl   $0x0,-0x4(%rbp)
ffffffff800007ea:	79 ad                	jns    ffffffff80000799 <TerminalPrintHex64+0x2a>
ffffffff800007ec:	be ff ff ff ff       	mov    $0xffffffff,%esi
ffffffff800007f1:	bf 0a 00 00 00       	mov    $0xa,%edi
ffffffff800007f6:	e8 0b fd ff ff       	call   ffffffff80000506 <TerminalPutChar32>
ffffffff800007fb:	90                   	nop
ffffffff800007fc:	c9                   	leave
ffffffff800007fd:	c3                   	ret

ffffffff800007fe <IDTSetGate>:
ffffffff800007fe:	f3 0f 1e fa          	endbr64
ffffffff80000802:	55                   	push   %rbp
ffffffff80000803:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000806:	48 83 ec 18          	sub    $0x18,%rsp
ffffffff8000080a:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
ffffffff8000080e:	89 c8                	mov    %ecx,%eax
ffffffff80000810:	89 f9                	mov    %edi,%ecx
ffffffff80000812:	88 4d fc             	mov    %cl,-0x4(%rbp)
ffffffff80000815:	66 89 55 f8          	mov    %dx,-0x8(%rbp)
ffffffff80000819:	88 45 ec             	mov    %al,-0x14(%rbp)
ffffffff8000081c:	0f b6 45 fc          	movzbl -0x4(%rbp),%eax
ffffffff80000820:	48 8b 55 f0          	mov    -0x10(%rbp),%rdx
ffffffff80000824:	48 98                	cltq
ffffffff80000826:	48 c1 e0 04          	shl    $0x4,%rax
ffffffff8000082a:	48 05 40 40 00 80    	add    $0xffffffff80004040,%rax
ffffffff80000830:	66 89 10             	mov    %dx,(%rax)
ffffffff80000833:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
ffffffff80000837:	48 c1 e8 10          	shr    $0x10,%rax
ffffffff8000083b:	48 89 c2             	mov    %rax,%rdx
ffffffff8000083e:	0f b6 45 fc          	movzbl -0x4(%rbp),%eax
ffffffff80000842:	48 98                	cltq
ffffffff80000844:	48 c1 e0 04          	shl    $0x4,%rax
ffffffff80000848:	48 05 46 40 00 80    	add    $0xffffffff80004046,%rax
ffffffff8000084e:	66 89 10             	mov    %dx,(%rax)
ffffffff80000851:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
ffffffff80000855:	48 c1 e8 20          	shr    $0x20,%rax
ffffffff80000859:	48 89 c2             	mov    %rax,%rdx
ffffffff8000085c:	0f b6 45 fc          	movzbl -0x4(%rbp),%eax
ffffffff80000860:	48 98                	cltq
ffffffff80000862:	48 c1 e0 04          	shl    $0x4,%rax
ffffffff80000866:	48 05 48 40 00 80    	add    $0xffffffff80004048,%rax
ffffffff8000086c:	89 10                	mov    %edx,(%rax)
ffffffff8000086e:	0f b6 45 fc          	movzbl -0x4(%rbp),%eax
ffffffff80000872:	48 98                	cltq
ffffffff80000874:	48 c1 e0 04          	shl    $0x4,%rax
ffffffff80000878:	48 8d 90 42 40 00 80 	lea    -0x7fffbfbe(%rax),%rdx
ffffffff8000087f:	0f b7 45 f8          	movzwl -0x8(%rbp),%eax
ffffffff80000883:	66 89 02             	mov    %ax,(%rdx)
ffffffff80000886:	0f b6 45 fc          	movzbl -0x4(%rbp),%eax
ffffffff8000088a:	48 98                	cltq
ffffffff8000088c:	48 c1 e0 04          	shl    $0x4,%rax
ffffffff80000890:	48 05 44 40 00 80    	add    $0xffffffff80004044,%rax
ffffffff80000896:	c6 00 00             	movb   $0x0,(%rax)
ffffffff80000899:	0f b6 45 fc          	movzbl -0x4(%rbp),%eax
ffffffff8000089d:	48 98                	cltq
ffffffff8000089f:	48 c1 e0 04          	shl    $0x4,%rax
ffffffff800008a3:	48 8d 90 45 40 00 80 	lea    -0x7fffbfbb(%rax),%rdx
ffffffff800008aa:	0f b6 45 ec          	movzbl -0x14(%rbp),%eax
ffffffff800008ae:	88 02                	mov    %al,(%rdx)
ffffffff800008b0:	0f b6 45 fc          	movzbl -0x4(%rbp),%eax
ffffffff800008b4:	48 98                	cltq
ffffffff800008b6:	48 c1 e0 04          	shl    $0x4,%rax
ffffffff800008ba:	48 05 4c 40 00 80    	add    $0xffffffff8000404c,%rax
ffffffff800008c0:	c7 00 00 00 00 00    	movl   $0x0,(%rax)
ffffffff800008c6:	90                   	nop
ffffffff800008c7:	c9                   	leave
ffffffff800008c8:	c3                   	ret

ffffffff800008c9 <IDTInitialize>:
ffffffff800008c9:	f3 0f 1e fa          	endbr64
ffffffff800008cd:	55                   	push   %rbp
ffffffff800008ce:	48 89 e5             	mov    %rsp,%rbp
ffffffff800008d1:	ba 00 10 00 00       	mov    $0x1000,%edx
ffffffff800008d6:	be 00 00 00 00       	mov    $0x0,%esi
ffffffff800008db:	48 c7 c7 40 40 00 80 	mov    $0xffffffff80004040,%rdi
ffffffff800008e2:	e8 e1 04 00 00       	call   ffffffff80000dc8 <memset>
ffffffff800008e7:	66 c7 05 50 47 00 00 	movw   $0xfff,0x4750(%rip)        # ffffffff80005040 <IDTP>
ffffffff800008ee:	ff 0f 
ffffffff800008f0:	48 c7 c0 40 40 00 80 	mov    $0xffffffff80004040,%rax
ffffffff800008f7:	48 89 05 44 47 00 00 	mov    %rax,0x4744(%rip)        # ffffffff80005042 <IDTP+0x2>
ffffffff800008fe:	0f 01 1d 3b 47 00 00 	lidt   0x473b(%rip)        # ffffffff80005040 <IDTP>
ffffffff80000905:	90                   	nop
ffffffff80000906:	5d                   	pop    %rbp
ffffffff80000907:	c3                   	ret

ffffffff80000908 <ISRHandler>:
ffffffff80000908:	f3 0f 1e fa          	endbr64
ffffffff8000090c:	55                   	push   %rbp
ffffffff8000090d:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000910:	48 83 ec 08          	sub    $0x8,%rsp
ffffffff80000914:	89 7d fc             	mov    %edi,-0x4(%rbp)
ffffffff80000917:	83 7d fc 1f          	cmpl   $0x1f,-0x4(%rbp)
ffffffff8000091b:	77 04                	ja     ffffffff80000921 <ISRHandler+0x19>
ffffffff8000091d:	fa                   	cli
ffffffff8000091e:	f4                   	hlt
ffffffff8000091f:	eb fd                	jmp    ffffffff8000091e <ISRHandler+0x16>
ffffffff80000921:	90                   	nop
ffffffff80000922:	c9                   	leave
ffffffff80000923:	c3                   	ret

ffffffff80000924 <ISRInitialize>:
ffffffff80000924:	f3 0f 1e fa          	endbr64
ffffffff80000928:	55                   	push   %rbp
ffffffff80000929:	48 89 e5             	mov    %rsp,%rbp
ffffffff8000092c:	48 c7 c0 c0 10 00 80 	mov    $0xffffffff800010c0,%rax
ffffffff80000933:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000938:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff8000093d:	48 89 c6             	mov    %rax,%rsi
ffffffff80000940:	bf 00 00 00 00       	mov    $0x0,%edi
ffffffff80000945:	e8 b4 fe ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff8000094a:	48 c7 c0 d6 10 00 80 	mov    $0xffffffff800010d6,%rax
ffffffff80000951:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000956:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff8000095b:	48 89 c6             	mov    %rax,%rsi
ffffffff8000095e:	bf 0e 00 00 00       	mov    $0xe,%edi
ffffffff80000963:	e8 96 fe ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000968:	90                   	nop
ffffffff80000969:	5d                   	pop    %rbp
ffffffff8000096a:	c3                   	ret

ffffffff8000096b <IRQHandler>:
ffffffff8000096b:	f3 0f 1e fa          	endbr64
ffffffff8000096f:	55                   	push   %rbp
ffffffff80000970:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000973:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000977:	89 7d fc             	mov    %edi,-0x4(%rbp)
ffffffff8000097a:	83 7d fc 01          	cmpl   $0x1,-0x4(%rbp)
ffffffff8000097e:	75 14                	jne    ffffffff80000994 <IRQHandler+0x29>
ffffffff80000980:	be 00 00 ff 00       	mov    $0xff0000,%esi
ffffffff80000985:	bf 4b 00 00 00       	mov    $0x4b,%edi
ffffffff8000098a:	e8 77 fb ff ff       	call   ffffffff80000506 <TerminalPutChar32>
ffffffff8000098f:	e8 20 06 00 00       	call   ffffffff80000fb4 <KeyboardInterrupt>
ffffffff80000994:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff80000997:	0f b6 c0             	movzbl %al,%eax
ffffffff8000099a:	89 c7                	mov    %eax,%edi
ffffffff8000099c:	e8 11 02 00 00       	call   ffffffff80000bb2 <PICSendEOI>
ffffffff800009a1:	90                   	nop
ffffffff800009a2:	c9                   	leave
ffffffff800009a3:	c3                   	ret

ffffffff800009a4 <IRQInitialize>:
ffffffff800009a4:	f3 0f 1e fa          	endbr64
ffffffff800009a8:	55                   	push   %rbp
ffffffff800009a9:	48 89 e5             	mov    %rsp,%rbp
ffffffff800009ac:	be 28 00 00 00       	mov    $0x28,%esi
ffffffff800009b1:	bf 20 00 00 00       	mov    $0x20,%edi
ffffffff800009b6:	e8 2f 02 00 00       	call   ffffffff80000bea <PICRemap>
ffffffff800009bb:	48 c7 c0 10 11 00 80 	mov    $0xffffffff80001110,%rax
ffffffff800009c2:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff800009c7:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff800009cc:	48 89 c6             	mov    %rax,%rsi
ffffffff800009cf:	bf 20 00 00 00       	mov    $0x20,%edi
ffffffff800009d4:	e8 25 fe ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff800009d9:	48 c7 c0 f0 10 00 80 	mov    $0xffffffff800010f0,%rax
ffffffff800009e0:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff800009e5:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff800009ea:	48 89 c6             	mov    %rax,%rsi
ffffffff800009ed:	bf 21 00 00 00       	mov    $0x21,%edi
ffffffff800009f2:	e8 07 fe ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff800009f7:	48 c7 c0 2f 11 00 80 	mov    $0xffffffff8000112f,%rax
ffffffff800009fe:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000a03:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000a08:	48 89 c6             	mov    %rax,%rsi
ffffffff80000a0b:	bf 22 00 00 00       	mov    $0x22,%edi
ffffffff80000a10:	e8 e9 fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000a15:	48 c7 c0 4e 11 00 80 	mov    $0xffffffff8000114e,%rax
ffffffff80000a1c:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000a21:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000a26:	48 89 c6             	mov    %rax,%rsi
ffffffff80000a29:	bf 23 00 00 00       	mov    $0x23,%edi
ffffffff80000a2e:	e8 cb fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000a33:	48 c7 c0 6d 11 00 80 	mov    $0xffffffff8000116d,%rax
ffffffff80000a3a:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000a3f:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000a44:	48 89 c6             	mov    %rax,%rsi
ffffffff80000a47:	bf 24 00 00 00       	mov    $0x24,%edi
ffffffff80000a4c:	e8 ad fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000a51:	48 c7 c0 8c 11 00 80 	mov    $0xffffffff8000118c,%rax
ffffffff80000a58:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000a5d:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000a62:	48 89 c6             	mov    %rax,%rsi
ffffffff80000a65:	bf 25 00 00 00       	mov    $0x25,%edi
ffffffff80000a6a:	e8 8f fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000a6f:	48 c7 c0 ab 11 00 80 	mov    $0xffffffff800011ab,%rax
ffffffff80000a76:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000a7b:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000a80:	48 89 c6             	mov    %rax,%rsi
ffffffff80000a83:	bf 26 00 00 00       	mov    $0x26,%edi
ffffffff80000a88:	e8 71 fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000a8d:	48 c7 c0 ca 11 00 80 	mov    $0xffffffff800011ca,%rax
ffffffff80000a94:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000a99:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000a9e:	48 89 c6             	mov    %rax,%rsi
ffffffff80000aa1:	bf 27 00 00 00       	mov    $0x27,%edi
ffffffff80000aa6:	e8 53 fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000aab:	48 c7 c0 e9 11 00 80 	mov    $0xffffffff800011e9,%rax
ffffffff80000ab2:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000ab7:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000abc:	48 89 c6             	mov    %rax,%rsi
ffffffff80000abf:	bf 28 00 00 00       	mov    $0x28,%edi
ffffffff80000ac4:	e8 35 fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000ac9:	48 c7 c0 08 12 00 80 	mov    $0xffffffff80001208,%rax
ffffffff80000ad0:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000ad5:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000ada:	48 89 c6             	mov    %rax,%rsi
ffffffff80000add:	bf 29 00 00 00       	mov    $0x29,%edi
ffffffff80000ae2:	e8 17 fd ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000ae7:	48 c7 c0 27 12 00 80 	mov    $0xffffffff80001227,%rax
ffffffff80000aee:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000af3:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000af8:	48 89 c6             	mov    %rax,%rsi
ffffffff80000afb:	bf 2a 00 00 00       	mov    $0x2a,%edi
ffffffff80000b00:	e8 f9 fc ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000b05:	48 c7 c0 46 12 00 80 	mov    $0xffffffff80001246,%rax
ffffffff80000b0c:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000b11:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000b16:	48 89 c6             	mov    %rax,%rsi
ffffffff80000b19:	bf 2b 00 00 00       	mov    $0x2b,%edi
ffffffff80000b1e:	e8 db fc ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000b23:	48 c7 c0 65 12 00 80 	mov    $0xffffffff80001265,%rax
ffffffff80000b2a:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000b2f:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000b34:	48 89 c6             	mov    %rax,%rsi
ffffffff80000b37:	bf 2c 00 00 00       	mov    $0x2c,%edi
ffffffff80000b3c:	e8 bd fc ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000b41:	48 c7 c0 84 12 00 80 	mov    $0xffffffff80001284,%rax
ffffffff80000b48:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000b4d:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000b52:	48 89 c6             	mov    %rax,%rsi
ffffffff80000b55:	bf 2d 00 00 00       	mov    $0x2d,%edi
ffffffff80000b5a:	e8 9f fc ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000b5f:	48 c7 c0 a3 12 00 80 	mov    $0xffffffff800012a3,%rax
ffffffff80000b66:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000b6b:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000b70:	48 89 c6             	mov    %rax,%rsi
ffffffff80000b73:	bf 2e 00 00 00       	mov    $0x2e,%edi
ffffffff80000b78:	e8 81 fc ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000b7d:	48 c7 c0 c2 12 00 80 	mov    $0xffffffff800012c2,%rax
ffffffff80000b84:	b9 8e 00 00 00       	mov    $0x8e,%ecx
ffffffff80000b89:	ba 08 00 00 00       	mov    $0x8,%edx
ffffffff80000b8e:	48 89 c6             	mov    %rax,%rsi
ffffffff80000b91:	bf 2f 00 00 00       	mov    $0x2f,%edi
ffffffff80000b96:	e8 63 fc ff ff       	call   ffffffff800007fe <IDTSetGate>
ffffffff80000b9b:	bf 00 00 00 00       	mov    $0x0,%edi
ffffffff80000ba0:	e8 12 01 00 00       	call   ffffffff80000cb7 <PICMaskIRQ>
ffffffff80000ba5:	bf 01 00 00 00       	mov    $0x1,%edi
ffffffff80000baa:	e8 6d 01 00 00       	call   ffffffff80000d1c <PICUnmaskIRQ>
ffffffff80000baf:	90                   	nop
ffffffff80000bb0:	5d                   	pop    %rbp
ffffffff80000bb1:	c3                   	ret

ffffffff80000bb2 <PICSendEOI>:
ffffffff80000bb2:	f3 0f 1e fa          	endbr64
ffffffff80000bb6:	55                   	push   %rbp
ffffffff80000bb7:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000bba:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000bbe:	89 f8                	mov    %edi,%eax
ffffffff80000bc0:	88 45 fc             	mov    %al,-0x4(%rbp)
ffffffff80000bc3:	80 7d fc 07          	cmpb   $0x7,-0x4(%rbp)
ffffffff80000bc7:	76 0f                	jbe    ffffffff80000bd8 <PICSendEOI+0x26>
ffffffff80000bc9:	be 20 00 00 00       	mov    $0x20,%esi
ffffffff80000bce:	bf a0 00 00 00       	mov    $0xa0,%edi
ffffffff80000bd3:	e8 cd 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000bd8:	be 20 00 00 00       	mov    $0x20,%esi
ffffffff80000bdd:	bf 20 00 00 00       	mov    $0x20,%edi
ffffffff80000be2:	e8 be 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000be7:	90                   	nop
ffffffff80000be8:	c9                   	leave
ffffffff80000be9:	c3                   	ret

ffffffff80000bea <PICRemap>:
ffffffff80000bea:	f3 0f 1e fa          	endbr64
ffffffff80000bee:	55                   	push   %rbp
ffffffff80000bef:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000bf2:	48 83 ec 20          	sub    $0x20,%rsp
ffffffff80000bf6:	89 fa                	mov    %edi,%edx
ffffffff80000bf8:	89 f0                	mov    %esi,%eax
ffffffff80000bfa:	88 55 ec             	mov    %dl,-0x14(%rbp)
ffffffff80000bfd:	88 45 e8             	mov    %al,-0x18(%rbp)
ffffffff80000c00:	bf 21 00 00 00       	mov    $0x21,%edi
ffffffff80000c05:	e8 79 01 00 00       	call   ffffffff80000d83 <Inb>
ffffffff80000c0a:	88 45 ff             	mov    %al,-0x1(%rbp)
ffffffff80000c0d:	bf a1 00 00 00       	mov    $0xa1,%edi
ffffffff80000c12:	e8 6c 01 00 00       	call   ffffffff80000d83 <Inb>
ffffffff80000c17:	88 45 fe             	mov    %al,-0x2(%rbp)
ffffffff80000c1a:	be 11 00 00 00       	mov    $0x11,%esi
ffffffff80000c1f:	bf 20 00 00 00       	mov    $0x20,%edi
ffffffff80000c24:	e8 7c 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c29:	be 11 00 00 00       	mov    $0x11,%esi
ffffffff80000c2e:	bf a0 00 00 00       	mov    $0xa0,%edi
ffffffff80000c33:	e8 6d 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c38:	0f b6 45 ec          	movzbl -0x14(%rbp),%eax
ffffffff80000c3c:	89 c6                	mov    %eax,%esi
ffffffff80000c3e:	bf 21 00 00 00       	mov    $0x21,%edi
ffffffff80000c43:	e8 5d 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c48:	0f b6 45 e8          	movzbl -0x18(%rbp),%eax
ffffffff80000c4c:	89 c6                	mov    %eax,%esi
ffffffff80000c4e:	bf a1 00 00 00       	mov    $0xa1,%edi
ffffffff80000c53:	e8 4d 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c58:	be 04 00 00 00       	mov    $0x4,%esi
ffffffff80000c5d:	bf 21 00 00 00       	mov    $0x21,%edi
ffffffff80000c62:	e8 3e 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c67:	be 02 00 00 00       	mov    $0x2,%esi
ffffffff80000c6c:	bf a1 00 00 00       	mov    $0xa1,%edi
ffffffff80000c71:	e8 2f 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c76:	be 01 00 00 00       	mov    $0x1,%esi
ffffffff80000c7b:	bf 21 00 00 00       	mov    $0x21,%edi
ffffffff80000c80:	e8 20 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c85:	be 01 00 00 00       	mov    $0x1,%esi
ffffffff80000c8a:	bf a1 00 00 00       	mov    $0xa1,%edi
ffffffff80000c8f:	e8 11 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000c94:	0f b6 45 ff          	movzbl -0x1(%rbp),%eax
ffffffff80000c98:	89 c6                	mov    %eax,%esi
ffffffff80000c9a:	bf 21 00 00 00       	mov    $0x21,%edi
ffffffff80000c9f:	e8 01 01 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000ca4:	0f b6 45 fe          	movzbl -0x2(%rbp),%eax
ffffffff80000ca8:	89 c6                	mov    %eax,%esi
ffffffff80000caa:	bf a1 00 00 00       	mov    $0xa1,%edi
ffffffff80000caf:	e8 f1 00 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000cb4:	90                   	nop
ffffffff80000cb5:	c9                   	leave
ffffffff80000cb6:	c3                   	ret

ffffffff80000cb7 <PICMaskIRQ>:
ffffffff80000cb7:	f3 0f 1e fa          	endbr64
ffffffff80000cbb:	55                   	push   %rbp
ffffffff80000cbc:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000cbf:	48 83 ec 20          	sub    $0x20,%rsp
ffffffff80000cc3:	89 f8                	mov    %edi,%eax
ffffffff80000cc5:	88 45 ec             	mov    %al,-0x14(%rbp)
ffffffff80000cc8:	80 7d ec 07          	cmpb   $0x7,-0x14(%rbp)
ffffffff80000ccc:	77 08                	ja     ffffffff80000cd6 <PICMaskIRQ+0x1f>
ffffffff80000cce:	66 c7 45 fe 21 00    	movw   $0x21,-0x2(%rbp)
ffffffff80000cd4:	eb 0a                	jmp    ffffffff80000ce0 <PICMaskIRQ+0x29>
ffffffff80000cd6:	66 c7 45 fe a1 00    	movw   $0xa1,-0x2(%rbp)
ffffffff80000cdc:	80 6d ec 08          	subb   $0x8,-0x14(%rbp)
ffffffff80000ce0:	0f b7 45 fe          	movzwl -0x2(%rbp),%eax
ffffffff80000ce4:	89 c7                	mov    %eax,%edi
ffffffff80000ce6:	e8 98 00 00 00       	call   ffffffff80000d83 <Inb>
ffffffff80000ceb:	88 45 fd             	mov    %al,-0x3(%rbp)
ffffffff80000cee:	0f b6 45 ec          	movzbl -0x14(%rbp),%eax
ffffffff80000cf2:	ba 01 00 00 00       	mov    $0x1,%edx
ffffffff80000cf7:	89 c1                	mov    %eax,%ecx
ffffffff80000cf9:	d3 e2                	shl    %cl,%edx
ffffffff80000cfb:	89 d0                	mov    %edx,%eax
ffffffff80000cfd:	89 c2                	mov    %eax,%edx
ffffffff80000cff:	0f b6 45 fd          	movzbl -0x3(%rbp),%eax
ffffffff80000d03:	09 d0                	or     %edx,%eax
ffffffff80000d05:	88 45 fd             	mov    %al,-0x3(%rbp)
ffffffff80000d08:	0f b6 55 fd          	movzbl -0x3(%rbp),%edx
ffffffff80000d0c:	0f b7 45 fe          	movzwl -0x2(%rbp),%eax
ffffffff80000d10:	89 d6                	mov    %edx,%esi
ffffffff80000d12:	89 c7                	mov    %eax,%edi
ffffffff80000d14:	e8 8c 00 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000d19:	90                   	nop
ffffffff80000d1a:	c9                   	leave
ffffffff80000d1b:	c3                   	ret

ffffffff80000d1c <PICUnmaskIRQ>:
ffffffff80000d1c:	f3 0f 1e fa          	endbr64
ffffffff80000d20:	55                   	push   %rbp
ffffffff80000d21:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000d24:	48 83 ec 20          	sub    $0x20,%rsp
ffffffff80000d28:	89 f8                	mov    %edi,%eax
ffffffff80000d2a:	88 45 ec             	mov    %al,-0x14(%rbp)
ffffffff80000d2d:	80 7d ec 07          	cmpb   $0x7,-0x14(%rbp)
ffffffff80000d31:	77 08                	ja     ffffffff80000d3b <PICUnmaskIRQ+0x1f>
ffffffff80000d33:	66 c7 45 fe 21 00    	movw   $0x21,-0x2(%rbp)
ffffffff80000d39:	eb 0a                	jmp    ffffffff80000d45 <PICUnmaskIRQ+0x29>
ffffffff80000d3b:	66 c7 45 fe a1 00    	movw   $0xa1,-0x2(%rbp)
ffffffff80000d41:	80 6d ec 08          	subb   $0x8,-0x14(%rbp)
ffffffff80000d45:	0f b7 45 fe          	movzwl -0x2(%rbp),%eax
ffffffff80000d49:	89 c7                	mov    %eax,%edi
ffffffff80000d4b:	e8 33 00 00 00       	call   ffffffff80000d83 <Inb>
ffffffff80000d50:	88 45 fd             	mov    %al,-0x3(%rbp)
ffffffff80000d53:	0f b6 45 ec          	movzbl -0x14(%rbp),%eax
ffffffff80000d57:	ba 01 00 00 00       	mov    $0x1,%edx
ffffffff80000d5c:	89 c1                	mov    %eax,%ecx
ffffffff80000d5e:	d3 e2                	shl    %cl,%edx
ffffffff80000d60:	89 d0                	mov    %edx,%eax
ffffffff80000d62:	f7 d0                	not    %eax
ffffffff80000d64:	89 c2                	mov    %eax,%edx
ffffffff80000d66:	0f b6 45 fd          	movzbl -0x3(%rbp),%eax
ffffffff80000d6a:	21 d0                	and    %edx,%eax
ffffffff80000d6c:	88 45 fd             	mov    %al,-0x3(%rbp)
ffffffff80000d6f:	0f b6 55 fd          	movzbl -0x3(%rbp),%edx
ffffffff80000d73:	0f b7 45 fe          	movzwl -0x2(%rbp),%eax
ffffffff80000d77:	89 d6                	mov    %edx,%esi
ffffffff80000d79:	89 c7                	mov    %eax,%edi
ffffffff80000d7b:	e8 25 00 00 00       	call   ffffffff80000da5 <Outb>
ffffffff80000d80:	90                   	nop
ffffffff80000d81:	c9                   	leave
ffffffff80000d82:	c3                   	ret

ffffffff80000d83 <Inb>:
ffffffff80000d83:	f3 0f 1e fa          	endbr64
ffffffff80000d87:	55                   	push   %rbp
ffffffff80000d88:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000d8b:	48 83 ec 18          	sub    $0x18,%rsp
ffffffff80000d8f:	89 f8                	mov    %edi,%eax
ffffffff80000d91:	66 89 45 ec          	mov    %ax,-0x14(%rbp)
ffffffff80000d95:	0f b7 45 ec          	movzwl -0x14(%rbp),%eax
ffffffff80000d99:	89 c2                	mov    %eax,%edx
ffffffff80000d9b:	ec                   	in     (%dx),%al
ffffffff80000d9c:	88 45 ff             	mov    %al,-0x1(%rbp)
ffffffff80000d9f:	0f b6 45 ff          	movzbl -0x1(%rbp),%eax
ffffffff80000da3:	c9                   	leave
ffffffff80000da4:	c3                   	ret

ffffffff80000da5 <Outb>:
ffffffff80000da5:	f3 0f 1e fa          	endbr64
ffffffff80000da9:	55                   	push   %rbp
ffffffff80000daa:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000dad:	48 83 ec 08          	sub    $0x8,%rsp
ffffffff80000db1:	89 fa                	mov    %edi,%edx
ffffffff80000db3:	89 f0                	mov    %esi,%eax
ffffffff80000db5:	66 89 55 fc          	mov    %dx,-0x4(%rbp)
ffffffff80000db9:	88 45 f8             	mov    %al,-0x8(%rbp)
ffffffff80000dbc:	0f b6 45 f8          	movzbl -0x8(%rbp),%eax
ffffffff80000dc0:	0f b7 55 fc          	movzwl -0x4(%rbp),%edx
ffffffff80000dc4:	ee                   	out    %al,(%dx)
ffffffff80000dc5:	90                   	nop
ffffffff80000dc6:	c9                   	leave
ffffffff80000dc7:	c3                   	ret

ffffffff80000dc8 <memset>:
ffffffff80000dc8:	f3 0f 1e fa          	endbr64
ffffffff80000dcc:	55                   	push   %rbp
ffffffff80000dcd:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000dd0:	48 83 ec 28          	sub    $0x28,%rsp
ffffffff80000dd4:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
ffffffff80000dd8:	89 75 e4             	mov    %esi,-0x1c(%rbp)
ffffffff80000ddb:	48 89 55 d8          	mov    %rdx,-0x28(%rbp)
ffffffff80000ddf:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
ffffffff80000de3:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
ffffffff80000de7:	eb 11                	jmp    ffffffff80000dfa <memset+0x32>
ffffffff80000de9:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
ffffffff80000ded:	48 8d 50 01          	lea    0x1(%rax),%rdx
ffffffff80000df1:	48 89 55 f8          	mov    %rdx,-0x8(%rbp)
ffffffff80000df5:	8b 55 e4             	mov    -0x1c(%rbp),%edx
ffffffff80000df8:	88 10                	mov    %dl,(%rax)
ffffffff80000dfa:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
ffffffff80000dfe:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
ffffffff80000e02:	48 89 55 d8          	mov    %rdx,-0x28(%rbp)
ffffffff80000e06:	48 85 c0             	test   %rax,%rax
ffffffff80000e09:	75 de                	jne    ffffffff80000de9 <memset+0x21>
ffffffff80000e0b:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
ffffffff80000e0f:	c9                   	leave
ffffffff80000e10:	c3                   	ret

ffffffff80000e11 <KbdInitialize>:
ffffffff80000e11:	f3 0f 1e fa          	endbr64
ffffffff80000e15:	55                   	push   %rbp
ffffffff80000e16:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000e19:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000e1d:	c7 05 39 42 00 00 00 	movl   $0x0,0x4239(%rip)        # ffffffff80005060 <KbdHead>
ffffffff80000e24:	00 00 00 
ffffffff80000e27:	c7 05 33 42 00 00 00 	movl   $0x0,0x4233(%rip)        # ffffffff80005064 <KbdTail>
ffffffff80000e2e:	00 00 00 
ffffffff80000e31:	c6 05 c8 42 00 00 00 	movb   $0x0,0x42c8(%rip)        # ffffffff80005100 <KbdState>
ffffffff80000e38:	c6 05 c2 42 00 00 00 	movb   $0x0,0x42c2(%rip)        # ffffffff80005101 <KbdState+0x1>
ffffffff80000e3f:	c6 05 bc 42 00 00 00 	movb   $0x0,0x42bc(%rip)        # ffffffff80005102 <KbdState+0x2>
ffffffff80000e46:	c6 05 b6 42 00 00 00 	movb   $0x0,0x42b6(%rip)        # ffffffff80005103 <KbdState+0x3>
ffffffff80000e4d:	c6 05 b0 42 00 00 00 	movb   $0x0,0x42b0(%rip)        # ffffffff80005104 <KbdState+0x4>
ffffffff80000e54:	c6 05 aa 42 00 00 00 	movb   $0x0,0x42aa(%rip)        # ffffffff80005105 <KbdState+0x5>
ffffffff80000e5b:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
ffffffff80000e62:	eb 0e                	jmp    ffffffff80000e72 <KbdInitialize+0x61>
ffffffff80000e64:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff80000e67:	c6 80 80 50 00 80 00 	movb   $0x0,-0x7fffaf80(%rax)
ffffffff80000e6e:	83 45 fc 01          	addl   $0x1,-0x4(%rbp)
ffffffff80000e72:	83 7d fc 7f          	cmpl   $0x7f,-0x4(%rbp)
ffffffff80000e76:	76 ec                	jbe    ffffffff80000e64 <KbdInitialize+0x53>
ffffffff80000e78:	90                   	nop
ffffffff80000e79:	90                   	nop
ffffffff80000e7a:	c9                   	leave
ffffffff80000e7b:	c3                   	ret

ffffffff80000e7c <KbdBufferPush>:
ffffffff80000e7c:	f3 0f 1e fa          	endbr64
ffffffff80000e80:	55                   	push   %rbp
ffffffff80000e81:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000e84:	48 83 ec 18          	sub    $0x18,%rsp
ffffffff80000e88:	89 f8                	mov    %edi,%eax
ffffffff80000e8a:	88 45 ec             	mov    %al,-0x14(%rbp)
ffffffff80000e8d:	8b 05 cd 41 00 00    	mov    0x41cd(%rip),%eax        # ffffffff80005060 <KbdHead>
ffffffff80000e93:	83 c0 01             	add    $0x1,%eax
ffffffff80000e96:	83 e0 7f             	and    $0x7f,%eax
ffffffff80000e99:	89 45 fc             	mov    %eax,-0x4(%rbp)
ffffffff80000e9c:	8b 05 c2 41 00 00    	mov    0x41c2(%rip),%eax        # ffffffff80005064 <KbdTail>
ffffffff80000ea2:	39 45 fc             	cmp    %eax,-0x4(%rbp)
ffffffff80000ea5:	74 1d                	je     ffffffff80000ec4 <KbdBufferPush+0x48>
ffffffff80000ea7:	8b 05 b3 41 00 00    	mov    0x41b3(%rip),%eax        # ffffffff80005060 <KbdHead>
ffffffff80000ead:	89 c2                	mov    %eax,%edx
ffffffff80000eaf:	0f b6 45 ec          	movzbl -0x14(%rbp),%eax
ffffffff80000eb3:	88 82 80 50 00 80    	mov    %al,-0x7fffaf80(%rdx)
ffffffff80000eb9:	8b 45 fc             	mov    -0x4(%rbp),%eax
ffffffff80000ebc:	89 05 9e 41 00 00    	mov    %eax,0x419e(%rip)        # ffffffff80005060 <KbdHead>
ffffffff80000ec2:	eb 01                	jmp    ffffffff80000ec5 <KbdBufferPush+0x49>
ffffffff80000ec4:	90                   	nop
ffffffff80000ec5:	c9                   	leave
ffffffff80000ec6:	c3                   	ret

ffffffff80000ec7 <KbdBufferPop>:
ffffffff80000ec7:	f3 0f 1e fa          	endbr64
ffffffff80000ecb:	55                   	push   %rbp
ffffffff80000ecc:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000ecf:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000ed3:	8b 15 87 41 00 00    	mov    0x4187(%rip),%edx        # ffffffff80005060 <KbdHead>
ffffffff80000ed9:	8b 05 85 41 00 00    	mov    0x4185(%rip),%eax        # ffffffff80005064 <KbdTail>
ffffffff80000edf:	39 c2                	cmp    %eax,%edx
ffffffff80000ee1:	75 07                	jne    ffffffff80000eea <KbdBufferPop+0x23>
ffffffff80000ee3:	b8 00 00 00 00       	mov    $0x0,%eax
ffffffff80000ee8:	eb 28                	jmp    ffffffff80000f12 <KbdBufferPop+0x4b>
ffffffff80000eea:	8b 05 74 41 00 00    	mov    0x4174(%rip),%eax        # ffffffff80005064 <KbdTail>
ffffffff80000ef0:	89 c0                	mov    %eax,%eax
ffffffff80000ef2:	0f b6 80 80 50 00 80 	movzbl -0x7fffaf80(%rax),%eax
ffffffff80000ef9:	88 45 ff             	mov    %al,-0x1(%rbp)
ffffffff80000efc:	8b 05 62 41 00 00    	mov    0x4162(%rip),%eax        # ffffffff80005064 <KbdTail>
ffffffff80000f02:	83 c0 01             	add    $0x1,%eax
ffffffff80000f05:	83 e0 7f             	and    $0x7f,%eax
ffffffff80000f08:	89 05 56 41 00 00    	mov    %eax,0x4156(%rip)        # ffffffff80005064 <KbdTail>
ffffffff80000f0e:	0f b6 45 ff          	movzbl -0x1(%rbp),%eax
ffffffff80000f12:	c9                   	leave
ffffffff80000f13:	c3                   	ret

ffffffff80000f14 <KbdAvailable>:
ffffffff80000f14:	f3 0f 1e fa          	endbr64
ffffffff80000f18:	55                   	push   %rbp
ffffffff80000f19:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000f1c:	8b 15 3e 41 00 00    	mov    0x413e(%rip),%edx        # ffffffff80005060 <KbdHead>
ffffffff80000f22:	8b 05 3c 41 00 00    	mov    0x413c(%rip),%eax        # ffffffff80005064 <KbdTail>
ffffffff80000f28:	39 c2                	cmp    %eax,%edx
ffffffff80000f2a:	0f 95 c0             	setne  %al
ffffffff80000f2d:	5d                   	pop    %rbp
ffffffff80000f2e:	c3                   	ret

ffffffff80000f2f <KbdReadChar>:
ffffffff80000f2f:	f3 0f 1e fa          	endbr64
ffffffff80000f33:	55                   	push   %rbp
ffffffff80000f34:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000f37:	e8 8b ff ff ff       	call   ffffffff80000ec7 <KbdBufferPop>
ffffffff80000f3c:	5d                   	pop    %rbp
ffffffff80000f3d:	c3                   	ret

ffffffff80000f3e <KbdFlushBuffer>:
ffffffff80000f3e:	f3 0f 1e fa          	endbr64
ffffffff80000f42:	55                   	push   %rbp
ffffffff80000f43:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000f46:	c7 05 10 41 00 00 00 	movl   $0x0,0x4110(%rip)        # ffffffff80005060 <KbdHead>
ffffffff80000f4d:	00 00 00 
ffffffff80000f50:	c7 05 0a 41 00 00 00 	movl   $0x0,0x410a(%rip)        # ffffffff80005064 <KbdTail>
ffffffff80000f57:	00 00 00 
ffffffff80000f5a:	90                   	nop
ffffffff80000f5b:	5d                   	pop    %rbp
ffffffff80000f5c:	c3                   	ret

ffffffff80000f5d <KbdGetState>:
ffffffff80000f5d:	f3 0f 1e fa          	endbr64
ffffffff80000f61:	55                   	push   %rbp
ffffffff80000f62:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000f65:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000f69:	8b 05 91 41 00 00    	mov    0x4191(%rip),%eax        # ffffffff80005100 <KbdState>
ffffffff80000f6f:	89 45 fa             	mov    %eax,-0x6(%rbp)
ffffffff80000f72:	0f b7 05 8b 41 00 00 	movzwl 0x418b(%rip),%eax        # ffffffff80005104 <KbdState+0x4>
ffffffff80000f79:	66 89 45 fe          	mov    %ax,-0x2(%rbp)
ffffffff80000f7d:	b8 00 00 00 00       	mov    $0x0,%eax
ffffffff80000f82:	8b 55 fa             	mov    -0x6(%rbp),%edx
ffffffff80000f85:	89 d2                	mov    %edx,%edx
ffffffff80000f87:	48 b9 00 00 00 00 ff 	movabs $0xffffffff00000000,%rcx
ffffffff80000f8e:	ff ff ff 
ffffffff80000f91:	48 21 c8             	and    %rcx,%rax
ffffffff80000f94:	48 09 d0             	or     %rdx,%rax
ffffffff80000f97:	0f b7 55 fe          	movzwl -0x2(%rbp),%edx
ffffffff80000f9b:	0f b7 d2             	movzwl %dx,%edx
ffffffff80000f9e:	48 c1 e2 20          	shl    $0x20,%rdx
ffffffff80000fa2:	48 b9 ff ff ff ff 00 	movabs $0xffff0000ffffffff,%rcx
ffffffff80000fa9:	00 ff ff 
ffffffff80000fac:	48 21 c8             	and    %rcx,%rax
ffffffff80000faf:	48 09 d0             	or     %rdx,%rax
ffffffff80000fb2:	c9                   	leave
ffffffff80000fb3:	c3                   	ret

ffffffff80000fb4 <KeyboardInterrupt>:
ffffffff80000fb4:	f3 0f 1e fa          	endbr64
ffffffff80000fb8:	55                   	push   %rbp
ffffffff80000fb9:	48 89 e5             	mov    %rsp,%rbp
ffffffff80000fbc:	48 83 ec 10          	sub    $0x10,%rsp
ffffffff80000fc0:	bf 60 00 00 00       	mov    $0x60,%edi
ffffffff80000fc5:	e8 b9 fd ff ff       	call   ffffffff80000d83 <Inb>
ffffffff80000fca:	88 45 fe             	mov    %al,-0x2(%rbp)
ffffffff80000fcd:	80 7d fe 2a          	cmpb   $0x2a,-0x2(%rbp)
ffffffff80000fd1:	75 0c                	jne    ffffffff80000fdf <KeyboardInterrupt+0x2b>
ffffffff80000fd3:	c6 05 26 41 00 00 01 	movb   $0x1,0x4126(%rip)        # ffffffff80005100 <KbdState>
ffffffff80000fda:	e9 d6 00 00 00       	jmp    ffffffff800010b5 <KeyboardInterrupt+0x101>
ffffffff80000fdf:	80 7d fe aa          	cmpb   $0xaa,-0x2(%rbp)
ffffffff80000fe3:	75 0c                	jne    ffffffff80000ff1 <KeyboardInterrupt+0x3d>
ffffffff80000fe5:	c6 05 14 41 00 00 00 	movb   $0x0,0x4114(%rip)        # ffffffff80005100 <KbdState>
ffffffff80000fec:	e9 c4 00 00 00       	jmp    ffffffff800010b5 <KeyboardInterrupt+0x101>
ffffffff80000ff1:	80 7d fe 36          	cmpb   $0x36,-0x2(%rbp)
ffffffff80000ff5:	75 0c                	jne    ffffffff80001003 <KeyboardInterrupt+0x4f>
ffffffff80000ff7:	c6 05 02 41 00 00 01 	movb   $0x1,0x4102(%rip)        # ffffffff80005100 <KbdState>
ffffffff80000ffe:	e9 b2 00 00 00       	jmp    ffffffff800010b5 <KeyboardInterrupt+0x101>
ffffffff80001003:	80 7d fe b6          	cmpb   $0xb6,-0x2(%rbp)
ffffffff80001007:	75 0c                	jne    ffffffff80001015 <KeyboardInterrupt+0x61>
ffffffff80001009:	c6 05 f0 40 00 00 00 	movb   $0x0,0x40f0(%rip)        # ffffffff80005100 <KbdState>
ffffffff80001010:	e9 a0 00 00 00       	jmp    ffffffff800010b5 <KeyboardInterrupt+0x101>
ffffffff80001015:	80 7d fe 3a          	cmpb   $0x3a,-0x2(%rbp)
ffffffff80001019:	75 20                	jne    ffffffff8000103b <KeyboardInterrupt+0x87>
ffffffff8000101b:	0f b6 05 e1 40 00 00 	movzbl 0x40e1(%rip),%eax        # ffffffff80005103 <KbdState+0x3>
ffffffff80001022:	0f b6 c0             	movzbl %al,%eax
ffffffff80001025:	85 c0                	test   %eax,%eax
ffffffff80001027:	0f 95 c0             	setne  %al
ffffffff8000102a:	83 f0 01             	xor    $0x1,%eax
ffffffff8000102d:	0f b6 c0             	movzbl %al,%eax
ffffffff80001030:	83 e0 01             	and    $0x1,%eax
ffffffff80001033:	88 05 ca 40 00 00    	mov    %al,0x40ca(%rip)        # ffffffff80005103 <KbdState+0x3>
ffffffff80001039:	eb 7a                	jmp    ffffffff800010b5 <KeyboardInterrupt+0x101>
ffffffff8000103b:	0f b6 45 fe          	movzbl -0x2(%rbp),%eax
ffffffff8000103f:	84 c0                	test   %al,%al
ffffffff80001041:	78 6e                	js     ffffffff800010b1 <KeyboardInterrupt+0xfd>
ffffffff80001043:	0f b6 45 fe          	movzbl -0x2(%rbp),%eax
ffffffff80001047:	48 98                	cltq
ffffffff80001049:	0f b6 80 60 25 00 80 	movzbl -0x7fffdaa0(%rax),%eax
ffffffff80001050:	88 45 ff             	mov    %al,-0x1(%rbp)
ffffffff80001053:	80 7d ff 60          	cmpb   $0x60,-0x1(%rbp)
ffffffff80001057:	7e 2a                	jle    ffffffff80001083 <KeyboardInterrupt+0xcf>
ffffffff80001059:	80 7d ff 7a          	cmpb   $0x7a,-0x1(%rbp)
ffffffff8000105d:	7f 24                	jg     ffffffff80001083 <KeyboardInterrupt+0xcf>
ffffffff8000105f:	0f b6 15 9a 40 00 00 	movzbl 0x409a(%rip),%edx        # ffffffff80005100 <KbdState>
ffffffff80001066:	0f b6 05 96 40 00 00 	movzbl 0x4096(%rip),%eax        # ffffffff80005103 <KbdState+0x3>
ffffffff8000106d:	38 c2                	cmp    %al,%dl
ffffffff8000106f:	74 2d                	je     ffffffff8000109e <KeyboardInterrupt+0xea>
ffffffff80001071:	0f b6 45 fe          	movzbl -0x2(%rbp),%eax
ffffffff80001075:	48 98                	cltq
ffffffff80001077:	0f b6 80 e0 25 00 80 	movzbl -0x7fffda20(%rax),%eax
ffffffff8000107e:	88 45 ff             	mov    %al,-0x1(%rbp)
ffffffff80001081:	eb 1b                	jmp    ffffffff8000109e <KeyboardInterrupt+0xea>
ffffffff80001083:	0f b6 05 76 40 00 00 	movzbl 0x4076(%rip),%eax        # ffffffff80005100 <KbdState>
ffffffff8000108a:	84 c0                	test   %al,%al
ffffffff8000108c:	74 10                	je     ffffffff8000109e <KeyboardInterrupt+0xea>
ffffffff8000108e:	0f b6 45 fe          	movzbl -0x2(%rbp),%eax
ffffffff80001092:	48 98                	cltq
ffffffff80001094:	0f b6 80 e0 25 00 80 	movzbl -0x7fffda20(%rax),%eax
ffffffff8000109b:	88 45 ff             	mov    %al,-0x1(%rbp)
ffffffff8000109e:	80 7d ff 00          	cmpb   $0x0,-0x1(%rbp)
ffffffff800010a2:	74 10                	je     ffffffff800010b4 <KeyboardInterrupt+0x100>
ffffffff800010a4:	0f be 45 ff          	movsbl -0x1(%rbp),%eax
ffffffff800010a8:	89 c7                	mov    %eax,%edi
ffffffff800010aa:	e8 cd fd ff ff       	call   ffffffff80000e7c <KbdBufferPush>
ffffffff800010af:	eb 04                	jmp    ffffffff800010b5 <KeyboardInterrupt+0x101>
ffffffff800010b1:	90                   	nop
ffffffff800010b2:	eb 01                	jmp    ffffffff800010b5 <KeyboardInterrupt+0x101>
ffffffff800010b4:	90                   	nop
ffffffff800010b5:	c9                   	leave
ffffffff800010b6:	c3                   	ret
ffffffff800010b7:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
ffffffff800010be:	00 00 

ffffffff800010c0 <ISR0>:
ffffffff800010c0:	50                   	push   %rax
ffffffff800010c1:	51                   	push   %rcx
ffffffff800010c2:	52                   	push   %rdx
ffffffff800010c3:	56                   	push   %rsi
ffffffff800010c4:	57                   	push   %rdi
ffffffff800010c5:	bf 00 00 00 00       	mov    $0x0,%edi
ffffffff800010ca:	e8 39 f8 ff ff       	call   ffffffff80000908 <ISRHandler>
ffffffff800010cf:	5f                   	pop    %rdi
ffffffff800010d0:	5e                   	pop    %rsi
ffffffff800010d1:	5a                   	pop    %rdx
ffffffff800010d2:	59                   	pop    %rcx
ffffffff800010d3:	58                   	pop    %rax
ffffffff800010d4:	48 cf                	iretq

ffffffff800010d6 <ISR14>:
ffffffff800010d6:	50                   	push   %rax
ffffffff800010d7:	51                   	push   %rcx
ffffffff800010d8:	52                   	push   %rdx
ffffffff800010d9:	56                   	push   %rsi
ffffffff800010da:	57                   	push   %rdi
ffffffff800010db:	bf 0e 00 00 00       	mov    $0xe,%edi
ffffffff800010e0:	e8 23 f8 ff ff       	call   ffffffff80000908 <ISRHandler>
ffffffff800010e5:	5f                   	pop    %rdi
ffffffff800010e6:	5e                   	pop    %rsi
ffffffff800010e7:	5a                   	pop    %rdx
ffffffff800010e8:	59                   	pop    %rcx
ffffffff800010e9:	58                   	pop    %rax
ffffffff800010ea:	48 83 c4 08          	add    $0x8,%rsp
ffffffff800010ee:	48 cf                	iretq

ffffffff800010f0 <IRQ1>:
ffffffff800010f0:	fa                   	cli
ffffffff800010f1:	50                   	push   %rax
ffffffff800010f2:	53                   	push   %rbx
ffffffff800010f3:	51                   	push   %rcx
ffffffff800010f4:	52                   	push   %rdx
ffffffff800010f5:	56                   	push   %rsi
ffffffff800010f6:	57                   	push   %rdi
ffffffff800010f7:	55                   	push   %rbp
ffffffff800010f8:	bf 01 00 00 00       	mov    $0x1,%edi
ffffffff800010fd:	e8 69 f8 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001102:	5d                   	pop    %rbp
ffffffff80001103:	5f                   	pop    %rdi
ffffffff80001104:	5e                   	pop    %rsi
ffffffff80001105:	5a                   	pop    %rdx
ffffffff80001106:	59                   	pop    %rcx
ffffffff80001107:	5b                   	pop    %rbx
ffffffff80001108:	58                   	pop    %rax
ffffffff80001109:	b0 20                	mov    $0x20,%al
ffffffff8000110b:	e6 20                	out    %al,$0x20
ffffffff8000110d:	fb                   	sti
ffffffff8000110e:	48 cf                	iretq

ffffffff80001110 <IRQ0>:
ffffffff80001110:	fa                   	cli
ffffffff80001111:	50                   	push   %rax
ffffffff80001112:	53                   	push   %rbx
ffffffff80001113:	51                   	push   %rcx
ffffffff80001114:	52                   	push   %rdx
ffffffff80001115:	56                   	push   %rsi
ffffffff80001116:	57                   	push   %rdi
ffffffff80001117:	55                   	push   %rbp
ffffffff80001118:	bf 00 00 00 00       	mov    $0x0,%edi
ffffffff8000111d:	e8 49 f8 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001122:	5d                   	pop    %rbp
ffffffff80001123:	5f                   	pop    %rdi
ffffffff80001124:	5e                   	pop    %rsi
ffffffff80001125:	5a                   	pop    %rdx
ffffffff80001126:	59                   	pop    %rcx
ffffffff80001127:	5b                   	pop    %rbx
ffffffff80001128:	58                   	pop    %rax
ffffffff80001129:	b0 20                	mov    $0x20,%al
ffffffff8000112b:	e6 20                	out    %al,$0x20
ffffffff8000112d:	48 cf                	iretq

ffffffff8000112f <IRQ2>:
ffffffff8000112f:	fa                   	cli
ffffffff80001130:	50                   	push   %rax
ffffffff80001131:	53                   	push   %rbx
ffffffff80001132:	51                   	push   %rcx
ffffffff80001133:	52                   	push   %rdx
ffffffff80001134:	56                   	push   %rsi
ffffffff80001135:	57                   	push   %rdi
ffffffff80001136:	55                   	push   %rbp
ffffffff80001137:	bf 02 00 00 00       	mov    $0x2,%edi
ffffffff8000113c:	e8 2a f8 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001141:	5d                   	pop    %rbp
ffffffff80001142:	5f                   	pop    %rdi
ffffffff80001143:	5e                   	pop    %rsi
ffffffff80001144:	5a                   	pop    %rdx
ffffffff80001145:	59                   	pop    %rcx
ffffffff80001146:	5b                   	pop    %rbx
ffffffff80001147:	58                   	pop    %rax
ffffffff80001148:	b0 20                	mov    $0x20,%al
ffffffff8000114a:	e6 20                	out    %al,$0x20
ffffffff8000114c:	48 cf                	iretq

ffffffff8000114e <IRQ3>:
ffffffff8000114e:	fa                   	cli
ffffffff8000114f:	50                   	push   %rax
ffffffff80001150:	53                   	push   %rbx
ffffffff80001151:	51                   	push   %rcx
ffffffff80001152:	52                   	push   %rdx
ffffffff80001153:	56                   	push   %rsi
ffffffff80001154:	57                   	push   %rdi
ffffffff80001155:	55                   	push   %rbp
ffffffff80001156:	bf 03 00 00 00       	mov    $0x3,%edi
ffffffff8000115b:	e8 0b f8 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001160:	5d                   	pop    %rbp
ffffffff80001161:	5f                   	pop    %rdi
ffffffff80001162:	5e                   	pop    %rsi
ffffffff80001163:	5a                   	pop    %rdx
ffffffff80001164:	59                   	pop    %rcx
ffffffff80001165:	5b                   	pop    %rbx
ffffffff80001166:	58                   	pop    %rax
ffffffff80001167:	b0 20                	mov    $0x20,%al
ffffffff80001169:	e6 20                	out    %al,$0x20
ffffffff8000116b:	48 cf                	iretq

ffffffff8000116d <IRQ4>:
ffffffff8000116d:	fa                   	cli
ffffffff8000116e:	50                   	push   %rax
ffffffff8000116f:	53                   	push   %rbx
ffffffff80001170:	51                   	push   %rcx
ffffffff80001171:	52                   	push   %rdx
ffffffff80001172:	56                   	push   %rsi
ffffffff80001173:	57                   	push   %rdi
ffffffff80001174:	55                   	push   %rbp
ffffffff80001175:	bf 04 00 00 00       	mov    $0x4,%edi
ffffffff8000117a:	e8 ec f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff8000117f:	5d                   	pop    %rbp
ffffffff80001180:	5f                   	pop    %rdi
ffffffff80001181:	5e                   	pop    %rsi
ffffffff80001182:	5a                   	pop    %rdx
ffffffff80001183:	59                   	pop    %rcx
ffffffff80001184:	5b                   	pop    %rbx
ffffffff80001185:	58                   	pop    %rax
ffffffff80001186:	b0 20                	mov    $0x20,%al
ffffffff80001188:	e6 20                	out    %al,$0x20
ffffffff8000118a:	48 cf                	iretq

ffffffff8000118c <IRQ5>:
ffffffff8000118c:	fa                   	cli
ffffffff8000118d:	50                   	push   %rax
ffffffff8000118e:	53                   	push   %rbx
ffffffff8000118f:	51                   	push   %rcx
ffffffff80001190:	52                   	push   %rdx
ffffffff80001191:	56                   	push   %rsi
ffffffff80001192:	57                   	push   %rdi
ffffffff80001193:	55                   	push   %rbp
ffffffff80001194:	bf 05 00 00 00       	mov    $0x5,%edi
ffffffff80001199:	e8 cd f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff8000119e:	5d                   	pop    %rbp
ffffffff8000119f:	5f                   	pop    %rdi
ffffffff800011a0:	5e                   	pop    %rsi
ffffffff800011a1:	5a                   	pop    %rdx
ffffffff800011a2:	59                   	pop    %rcx
ffffffff800011a3:	5b                   	pop    %rbx
ffffffff800011a4:	58                   	pop    %rax
ffffffff800011a5:	b0 20                	mov    $0x20,%al
ffffffff800011a7:	e6 20                	out    %al,$0x20
ffffffff800011a9:	48 cf                	iretq

ffffffff800011ab <IRQ6>:
ffffffff800011ab:	fa                   	cli
ffffffff800011ac:	50                   	push   %rax
ffffffff800011ad:	53                   	push   %rbx
ffffffff800011ae:	51                   	push   %rcx
ffffffff800011af:	52                   	push   %rdx
ffffffff800011b0:	56                   	push   %rsi
ffffffff800011b1:	57                   	push   %rdi
ffffffff800011b2:	55                   	push   %rbp
ffffffff800011b3:	bf 06 00 00 00       	mov    $0x6,%edi
ffffffff800011b8:	e8 ae f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff800011bd:	5d                   	pop    %rbp
ffffffff800011be:	5f                   	pop    %rdi
ffffffff800011bf:	5e                   	pop    %rsi
ffffffff800011c0:	5a                   	pop    %rdx
ffffffff800011c1:	59                   	pop    %rcx
ffffffff800011c2:	5b                   	pop    %rbx
ffffffff800011c3:	58                   	pop    %rax
ffffffff800011c4:	b0 20                	mov    $0x20,%al
ffffffff800011c6:	e6 20                	out    %al,$0x20
ffffffff800011c8:	48 cf                	iretq

ffffffff800011ca <IRQ7>:
ffffffff800011ca:	fa                   	cli
ffffffff800011cb:	50                   	push   %rax
ffffffff800011cc:	53                   	push   %rbx
ffffffff800011cd:	51                   	push   %rcx
ffffffff800011ce:	52                   	push   %rdx
ffffffff800011cf:	56                   	push   %rsi
ffffffff800011d0:	57                   	push   %rdi
ffffffff800011d1:	55                   	push   %rbp
ffffffff800011d2:	bf 07 00 00 00       	mov    $0x7,%edi
ffffffff800011d7:	e8 8f f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff800011dc:	5d                   	pop    %rbp
ffffffff800011dd:	5f                   	pop    %rdi
ffffffff800011de:	5e                   	pop    %rsi
ffffffff800011df:	5a                   	pop    %rdx
ffffffff800011e0:	59                   	pop    %rcx
ffffffff800011e1:	5b                   	pop    %rbx
ffffffff800011e2:	58                   	pop    %rax
ffffffff800011e3:	b0 20                	mov    $0x20,%al
ffffffff800011e5:	e6 20                	out    %al,$0x20
ffffffff800011e7:	48 cf                	iretq

ffffffff800011e9 <IRQ8>:
ffffffff800011e9:	fa                   	cli
ffffffff800011ea:	50                   	push   %rax
ffffffff800011eb:	53                   	push   %rbx
ffffffff800011ec:	51                   	push   %rcx
ffffffff800011ed:	52                   	push   %rdx
ffffffff800011ee:	56                   	push   %rsi
ffffffff800011ef:	57                   	push   %rdi
ffffffff800011f0:	55                   	push   %rbp
ffffffff800011f1:	bf 08 00 00 00       	mov    $0x8,%edi
ffffffff800011f6:	e8 70 f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff800011fb:	5d                   	pop    %rbp
ffffffff800011fc:	5f                   	pop    %rdi
ffffffff800011fd:	5e                   	pop    %rsi
ffffffff800011fe:	5a                   	pop    %rdx
ffffffff800011ff:	59                   	pop    %rcx
ffffffff80001200:	5b                   	pop    %rbx
ffffffff80001201:	58                   	pop    %rax
ffffffff80001202:	b0 20                	mov    $0x20,%al
ffffffff80001204:	e6 20                	out    %al,$0x20
ffffffff80001206:	48 cf                	iretq

ffffffff80001208 <IRQ9>:
ffffffff80001208:	fa                   	cli
ffffffff80001209:	50                   	push   %rax
ffffffff8000120a:	53                   	push   %rbx
ffffffff8000120b:	51                   	push   %rcx
ffffffff8000120c:	52                   	push   %rdx
ffffffff8000120d:	56                   	push   %rsi
ffffffff8000120e:	57                   	push   %rdi
ffffffff8000120f:	55                   	push   %rbp
ffffffff80001210:	bf 09 00 00 00       	mov    $0x9,%edi
ffffffff80001215:	e8 51 f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff8000121a:	5d                   	pop    %rbp
ffffffff8000121b:	5f                   	pop    %rdi
ffffffff8000121c:	5e                   	pop    %rsi
ffffffff8000121d:	5a                   	pop    %rdx
ffffffff8000121e:	59                   	pop    %rcx
ffffffff8000121f:	5b                   	pop    %rbx
ffffffff80001220:	58                   	pop    %rax
ffffffff80001221:	b0 20                	mov    $0x20,%al
ffffffff80001223:	e6 20                	out    %al,$0x20
ffffffff80001225:	48 cf                	iretq

ffffffff80001227 <IRQ10>:
ffffffff80001227:	fa                   	cli
ffffffff80001228:	50                   	push   %rax
ffffffff80001229:	53                   	push   %rbx
ffffffff8000122a:	51                   	push   %rcx
ffffffff8000122b:	52                   	push   %rdx
ffffffff8000122c:	56                   	push   %rsi
ffffffff8000122d:	57                   	push   %rdi
ffffffff8000122e:	55                   	push   %rbp
ffffffff8000122f:	bf 0a 00 00 00       	mov    $0xa,%edi
ffffffff80001234:	e8 32 f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001239:	5d                   	pop    %rbp
ffffffff8000123a:	5f                   	pop    %rdi
ffffffff8000123b:	5e                   	pop    %rsi
ffffffff8000123c:	5a                   	pop    %rdx
ffffffff8000123d:	59                   	pop    %rcx
ffffffff8000123e:	5b                   	pop    %rbx
ffffffff8000123f:	58                   	pop    %rax
ffffffff80001240:	b0 20                	mov    $0x20,%al
ffffffff80001242:	e6 20                	out    %al,$0x20
ffffffff80001244:	48 cf                	iretq

ffffffff80001246 <IRQ11>:
ffffffff80001246:	fa                   	cli
ffffffff80001247:	50                   	push   %rax
ffffffff80001248:	53                   	push   %rbx
ffffffff80001249:	51                   	push   %rcx
ffffffff8000124a:	52                   	push   %rdx
ffffffff8000124b:	56                   	push   %rsi
ffffffff8000124c:	57                   	push   %rdi
ffffffff8000124d:	55                   	push   %rbp
ffffffff8000124e:	bf 0b 00 00 00       	mov    $0xb,%edi
ffffffff80001253:	e8 13 f7 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001258:	5d                   	pop    %rbp
ffffffff80001259:	5f                   	pop    %rdi
ffffffff8000125a:	5e                   	pop    %rsi
ffffffff8000125b:	5a                   	pop    %rdx
ffffffff8000125c:	59                   	pop    %rcx
ffffffff8000125d:	5b                   	pop    %rbx
ffffffff8000125e:	58                   	pop    %rax
ffffffff8000125f:	b0 20                	mov    $0x20,%al
ffffffff80001261:	e6 20                	out    %al,$0x20
ffffffff80001263:	48 cf                	iretq

ffffffff80001265 <IRQ12>:
ffffffff80001265:	fa                   	cli
ffffffff80001266:	50                   	push   %rax
ffffffff80001267:	53                   	push   %rbx
ffffffff80001268:	51                   	push   %rcx
ffffffff80001269:	52                   	push   %rdx
ffffffff8000126a:	56                   	push   %rsi
ffffffff8000126b:	57                   	push   %rdi
ffffffff8000126c:	55                   	push   %rbp
ffffffff8000126d:	bf 0c 00 00 00       	mov    $0xc,%edi
ffffffff80001272:	e8 f4 f6 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001277:	5d                   	pop    %rbp
ffffffff80001278:	5f                   	pop    %rdi
ffffffff80001279:	5e                   	pop    %rsi
ffffffff8000127a:	5a                   	pop    %rdx
ffffffff8000127b:	59                   	pop    %rcx
ffffffff8000127c:	5b                   	pop    %rbx
ffffffff8000127d:	58                   	pop    %rax
ffffffff8000127e:	b0 20                	mov    $0x20,%al
ffffffff80001280:	e6 20                	out    %al,$0x20
ffffffff80001282:	48 cf                	iretq

ffffffff80001284 <IRQ13>:
ffffffff80001284:	fa                   	cli
ffffffff80001285:	50                   	push   %rax
ffffffff80001286:	53                   	push   %rbx
ffffffff80001287:	51                   	push   %rcx
ffffffff80001288:	52                   	push   %rdx
ffffffff80001289:	56                   	push   %rsi
ffffffff8000128a:	57                   	push   %rdi
ffffffff8000128b:	55                   	push   %rbp
ffffffff8000128c:	bf 0d 00 00 00       	mov    $0xd,%edi
ffffffff80001291:	e8 d5 f6 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff80001296:	5d                   	pop    %rbp
ffffffff80001297:	5f                   	pop    %rdi
ffffffff80001298:	5e                   	pop    %rsi
ffffffff80001299:	5a                   	pop    %rdx
ffffffff8000129a:	59                   	pop    %rcx
ffffffff8000129b:	5b                   	pop    %rbx
ffffffff8000129c:	58                   	pop    %rax
ffffffff8000129d:	b0 20                	mov    $0x20,%al
ffffffff8000129f:	e6 20                	out    %al,$0x20
ffffffff800012a1:	48 cf                	iretq

ffffffff800012a3 <IRQ14>:
ffffffff800012a3:	fa                   	cli
ffffffff800012a4:	50                   	push   %rax
ffffffff800012a5:	53                   	push   %rbx
ffffffff800012a6:	51                   	push   %rcx
ffffffff800012a7:	52                   	push   %rdx
ffffffff800012a8:	56                   	push   %rsi
ffffffff800012a9:	57                   	push   %rdi
ffffffff800012aa:	55                   	push   %rbp
ffffffff800012ab:	bf 0e 00 00 00       	mov    $0xe,%edi
ffffffff800012b0:	e8 b6 f6 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff800012b5:	5d                   	pop    %rbp
ffffffff800012b6:	5f                   	pop    %rdi
ffffffff800012b7:	5e                   	pop    %rsi
ffffffff800012b8:	5a                   	pop    %rdx
ffffffff800012b9:	59                   	pop    %rcx
ffffffff800012ba:	5b                   	pop    %rbx
ffffffff800012bb:	58                   	pop    %rax
ffffffff800012bc:	b0 20                	mov    $0x20,%al
ffffffff800012be:	e6 20                	out    %al,$0x20
ffffffff800012c0:	48 cf                	iretq

ffffffff800012c2 <IRQ15>:
ffffffff800012c2:	fa                   	cli
ffffffff800012c3:	50                   	push   %rax
ffffffff800012c4:	53                   	push   %rbx
ffffffff800012c5:	51                   	push   %rcx
ffffffff800012c6:	52                   	push   %rdx
ffffffff800012c7:	56                   	push   %rsi
ffffffff800012c8:	57                   	push   %rdi
ffffffff800012c9:	55                   	push   %rbp
ffffffff800012ca:	bf 0f 00 00 00       	mov    $0xf,%edi
ffffffff800012cf:	e8 97 f6 ff ff       	call   ffffffff8000096b <IRQHandler>
ffffffff800012d4:	5d                   	pop    %rbp
ffffffff800012d5:	5f                   	pop    %rdi
ffffffff800012d6:	5e                   	pop    %rsi
ffffffff800012d7:	5a                   	pop    %rdx
ffffffff800012d8:	59                   	pop    %rcx
ffffffff800012d9:	5b                   	pop    %rbx
ffffffff800012da:	58                   	pop    %rax
ffffffff800012db:	b0 20                	mov    $0x20,%al
ffffffff800012dd:	e6 20                	out    %al,$0x20
ffffffff800012df:	48 cf                	iretq
