
#ident "@(#)intr.s	1.4	97/11/24 SMI"

#if !defined(lint)

	.text

	.globl	cmnint
	.globl	cmntrap

	.align	4
	.globl	div0trap
div0trap:
	pushl	$0
	pushl	$0
	jmp	cmntrap

	.align	4
	.globl	dbgtrap
dbgtrap:
nodbgmon:
	pushl	$0
	pushl	$1
	jmp	cmntrap

	.align	4
	.globl	nmiint
nmiint:
	pushl	$0
	pushl	$2
	jmp	nmitrap

	.align	4
	.globl	brktrap
brktrap:
	pushl	$0
	pushl	$3
	jmp	cmntrap

/	jmp	nobrkmon		/ for now......
/	.align	4
/nobrkmon:
/	pushl	$0
/	pushl	$3
/	jmp	cmntrap

	.align	4
	.globl	ovflotrap
ovflotrap:
	pushl	$0
	pushl	$4
	jmp	cmntrap

	.align	4
	.globl	boundstrap
boundstrap:
	pushl	$0
	pushl	$5
	jmp	cmntrap

	.align	4
	.globl	invoptrap
invoptrap:
	pushl	$0
	pushl	$6
	jmp	cmntrap

	.align	4
	.globl	ndptrap0

ndptrap0:
	pushl	$0
	pushl	$7
	jmp	cmntrap

	.align	4
	.globl	dbfault
dbfault:
	pushl   $8              / Trap number for default handler
	jmp	cmntrap

	.align	4
	.globl	overrun
overrun:
	pushl	$0
	pushl	$9
	jmp	cmntrap

	.align	4
	.globl	invtsstrap
invtsstrap:
	pushl	$10		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	segnptrap
segnptrap:
	pushl	$11		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	stktrap
stktrap:
	pushl	$12		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	gptrap
gptrap:
	pushl	$13		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	pftrap
pftrap:
	pushl	$14		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	resvtrap
resvtrap:
	pushl	$0
	pushl	$15
	jmp	cmntrap

	.align	4
	.globl	ndperr
ndperr:
	pushl	$0
	pushl	$16
	jmp	cmntrap

/	.align	4
/	.globl	invaltrap
/invaltrap:
/bh	pushl	$0			chip does this for use
/	pushl	$17
/	jmp	cmntrap

	.align	4
	.globl	inval17
inval17:
	pushl	$0
	pushl	$17
	jmp	cmnint

	.align	4
	.globl	inval18
inval18:
	pushl	$0
	pushl	$18
	jmp	cmnint

	.align	4
	.globl	inval19
inval19:
	pushl	$0
	pushl	$19
	jmp	cmnint

	.align	4
	.globl	progent
progent:
	pushl	$0
	pushl	$20
	jmp	cmnint

	.align	4
	.globl	inval21
inval21:
	pushl	$0
	pushl	$21
	jmp	cmnint

	.align	4
	.globl	inval22
inval22:
	pushl	$0
	pushl	$22
	jmp	cmnint

	.align	4
	.globl	inval23
inval23:
	pushl	$0
	pushl	$23
	jmp	cmnint

	.align	4
	.globl	inval24
inval24:
	pushl	$0
	pushl	$24
	jmp	cmnint

	.align	4
	.globl	inval25
inval25:
	pushl	$0
	pushl	$25
	jmp	cmnint

	.align	4
	.globl	inval26
inval26:
	pushl	$0
	pushl	$26
	jmp	cmnint

	.align	4
	.globl	inval27
inval27:
	pushl	$0
	pushl	$27
	jmp	cmnint

	.align	4
	.globl	inval28
inval28:
	pushl	$0
	pushl	$28
	jmp	cmnint

	.align	4
	.globl	inval29
inval29:
	pushl	$0
	pushl	$29
	jmp	cmnint

	.align	4
	.globl	inval30
inval30:
	pushl	$0
	pushl	$30
	jmp	cmnint

	.align	4
	.globl	inval31
inval31:
	pushl	$0
	pushl	$31
	jmp	cmnint

	.align	4
	.globl	ndptrap2
ndptrap2:
	pushl	$0
	pushl	$32
	jmp	cmntrap

	.align	4
	.globl	inval33
inval33:
	pushl	$0
	pushl	$33
	jmp	cmnint

	.align	4
	.globl	inval34
inval34:
	pushl	$0
	pushl	$34
	jmp	cmnint

	.align	4
	.globl	inval35
inval35:
	pushl	$0
	pushl	$35
	jmp	cmnint

	.align	4
	.globl	inval36
inval36:
	pushl	$0
	pushl	$36
	jmp	cmnint

	.align	4
	.globl	inval37
inval37:
	pushl	$0
	pushl	$37
	jmp	cmnint

	.align	4
	.globl	inval38
inval38:
	pushl	$0
	pushl	$38
	jmp	cmnint

	.align	4
	.globl	inval39
inval39:
	pushl	$0
	pushl	$39
	jmp	cmnint

	.align	4
	.globl	inval40
inval40:
	pushl	$0
	pushl	$40
	jmp	cmnint

	.align	4
	.globl	inval41
inval41:
	pushl	$0
	pushl	$41
	jmp	cmnint

	.align	4
	.globl	inval42
inval42:
	pushl	$0
	pushl	$42
	jmp	cmnint

	.align	4
	.globl	inval43
inval43:
	pushl	$0
	pushl	$43
	jmp	cmnint

	.align	4
	.globl	inval44
inval44:
	pushl	$0
	pushl	$44
	jmp	cmnint

	.align	4
	.globl	inval45
inval45:
	pushl	$0
	pushl	$45
	jmp	cmnint

	.align	4
	.globl	inval46
inval46:
	pushl	$0
	pushl	$46
	jmp	cmnint

	.align	4
	.globl	inval47
inval47:
	pushl	$0
	pushl	$47
	jmp	cmnint

	.align	4
	.globl	inval48
inval48:
	pushl	$0
	pushl	$48
	jmp	cmnint

	.align	4
	.globl	inval49
inval49:
	pushl	$0
	pushl	$49
	jmp	cmnint

	.align	4
	.globl	inval50
inval50:
	pushl	$0
	pushl	$50
	jmp	cmnint

	.align	4
	.globl	inval51
inval51:
	pushl	$0
	pushl	$51
	jmp	cmnint

	.align	4
	.globl	inval52
inval52:
	pushl	$0
	pushl	$52
	jmp	cmnint

	.align	4
	.globl	inval53
inval53:
	pushl	$0
	pushl	$53
	jmp	cmnint

	.align	4
	.globl	inval54
inval54:
	pushl	$0
	pushl	$54
	jmp	cmnint

	.align	4
	.globl	inval55
inval55:
	pushl	$0
	pushl	$55
	jmp	cmnint

	.align	4
	.globl	inval56
inval56:
	pushl	$0
	pushl	$56
	jmp	cmnint

	.align	4
	.globl	inval57
inval57:
	pushl	$0
	pushl	$57
	jmp	cmnint

	.align	4
	.globl	inval58
inval58:
	pushl	$0
	pushl	$58
	jmp	cmnint

	.align	4
	.globl	inval59
inval59:
	pushl	$0
	pushl	$59
	jmp	cmnint

	.align	4
	.globl	inval60
inval60:
	pushl	$0
	pushl	$60
	jmp	cmnint

	.align	4
	.globl	inval61
inval61:
	pushl	$0
	pushl	$61
	jmp	cmnint

	.align	4
	.globl	inval62
inval62:
	pushl	$0
	pushl	$62
	jmp	cmnint

	.align	4
	.globl	inval63
inval63:
	pushl	$0
	pushl	$63
	jmp	cmnint

	.align	4
	.globl	ivctM0
ivctM0:
	pushl	$0
	pushl	$64
	jmp	cmnint

	.align	4
	.globl	ivctM1
ivctM1:
	pushl	$0
	pushl	$65
	jmp	cmnint

	.align	4
	.globl	ivctM2
ivctM2:
	pushl	$0
	pushl	$66
	jmp	cmnint

	.align	4
	.globl	ivctM3
ivctM3:
	pushl	$0
	pushl	$67
	jmp	cmnint

	.align	4
	.globl	ivctM4
ivctM4:
	pushl	$0
	pushl	$68
	jmp	cmnint

	.align	4
	.globl	ivctM5
ivctM5:
	pushl	$0
	pushl	$69
	jmp	cmnint

	.align	4
	.globl	ivctM6
ivctM6:
	pushl	$0
	pushl	$70
	jmp	cmnint

	.align	4
	.globl	ivctM7
ivctM7:
	pushl	$0
	pushl	$71
	jmp	cmnint

	.align	4
	.globl	ivctM0S0
ivctM0S0:
	pushl	$0
	pushl	$72
	jmp	cmnint

	.align	4
	.globl	ivctM0S1
ivctM0S1:
	pushl	$0
	pushl	$73
	jmp	cmnint

	.align	4
	.globl	ivctM0S2
ivctM0S2:
	pushl	$0
	pushl	$74
	jmp	cmnint

	.align	4
	.globl	ivctM0S3
ivctM0S3:
	pushl	$0
	pushl	$75
	jmp	cmnint

	.align	4
	.globl	ivctM0S4
ivctM0S4:
	pushl	$0
	pushl	$76
	jmp	cmnint

	.align	4
	.globl	ivctM0S5
ivctM0S5:
	pushl	$0
	pushl	$77
	jmp	cmnint

	.align	4
	.globl	ivctM0S6
ivctM0S6:
	pushl	$0
	pushl	$78
	jmp	cmnint

	.align	4
	.globl	ivctM0S7
ivctM0S7:
	pushl	$0
	pushl	$79
	jmp	cmnint

	.align	4
	.globl	ivctM1S0
ivctM1S0:
	pushl	$0
	pushl	$80
	jmp	cmnint

	.align	4
	.globl	ivctM1S1
ivctM1S1:
	pushl	$0
	pushl	$81
	jmp	cmnint

	.align	4
	.globl	ivctM1S2
ivctM1S2:
	pushl	$0
	pushl	$82
	jmp	cmnint

	.align	4
	.globl	ivctM1S3
ivctM1S3:
	pushl	$0
	pushl	$83
	jmp	cmnint

	.align	4
	.globl	ivctM1S4
ivctM1S4:
	pushl	$0
	pushl	$84
	jmp	cmnint

	.align	4
	.globl	ivctM1S5
ivctM1S5:
	pushl	$0
	pushl	$85
	jmp	cmnint

	.align	4
	.globl	ivctM1S6
ivctM1S6:
	pushl	$0
	pushl	$86
	jmp	cmnint

	.align	4
	.globl	ivctM1S7
ivctM1S7:
	pushl	$0
	pushl	$87
	jmp	cmnint

	.align	4
	.globl	ivctM2S0
ivctM2S0:
	pushl	$0
	pushl	$88
	jmp	cmnint

	.align	4
	.globl	ivctM2S1
ivctM2S1:
	pushl	$0
	pushl	$89
	jmp	cmnint

	.align	4
	.globl	ivctM2S2
ivctM2S2:
	pushl	$0
	pushl	$90
	jmp	cmnint

	.align	4
	.globl	ivctM2S3
ivctM2S3:
	pushl	$0
	pushl	$91
	jmp	cmnint

	.align	4
	.globl	ivctM2S4
ivctM2S4:
	pushl	$0
	pushl	$92
	jmp	cmnint

	.align	4
	.globl	ivctM2S5
ivctM2S5:
	pushl	$0
	pushl	$93
	jmp	cmnint

	.align	4
	.globl	ivctM2S6
ivctM2S6:
	pushl	$0
	pushl	$94
	jmp	cmnint

	.align	4
	.globl	ivctM2S7
ivctM2S7:
	pushl	$0
	pushl	$95
	jmp	cmnint

	.align	4
	.globl	ivctM3S0
ivctM3S0:
	pushl	$0
	pushl	$96
	jmp	cmnint

	.align	4
	.globl	ivctM3S1
ivctM3S1:
	pushl	$0
	pushl	$97
	jmp	cmnint

	.align	4
	.globl	ivctM3S2
ivctM3S2:
	pushl	$0
	pushl	$98
	jmp	cmnint

	.align	4
	.globl	ivctM3S3
ivctM3S3:
	pushl	$0
	pushl	$99
	jmp	cmnint

	.align	4
	.globl	ivctM3S4
ivctM3S4:
	pushl	$0
	pushl	$100
	jmp	cmnint

	.align	4
	.globl	ivctM3S5
ivctM3S5:
	pushl	$0
	pushl	$101
	jmp	cmnint

	.align	4
	.globl	ivctM3S6
ivctM3S6:
	pushl	$0
	pushl	$102
	jmp	cmnint

	.align	4
	.globl	ivctM3S7
ivctM3S7:
	pushl	$0
	pushl	$103
	jmp	cmnint

	.align	4
	.globl	ivctM4S0
ivctM4S0:
	pushl	$0
	pushl	$104
	jmp	cmnint

	.align	4
	.globl	ivctM4S1
ivctM4S1:
	pushl	$0
	pushl	$105
	jmp	cmnint

	.align	4
	.globl	ivctM4S2
ivctM4S2:
	pushl	$0
	pushl	$106
	jmp	cmnint

	.align	4
	.globl	ivctM4S3
ivctM4S3:
	pushl	$0
	pushl	$107
	jmp	cmnint

	.align	4
	.globl	ivctM4S4
ivctM4S4:
	pushl	$0
	pushl	$108
	jmp	cmnint

	.align	4
	.globl	ivctM4S5
ivctM4S5:
	pushl	$0
	pushl	$109
	jmp	cmnint

	.align	4
	.globl	ivctM4S6
ivctM4S6:
	pushl	$0
	pushl	$110
	jmp	cmnint

	.align	4
	.globl	ivctM4S7
ivctM4S7:
	pushl	$0
	pushl	$111
	jmp	cmnint

	.align	4
	.globl	ivctM5S0
ivctM5S0:
	pushl	$0
	pushl	$112
	jmp	cmnint

	.align	4
	.globl	ivctM5S1
ivctM5S1:
	pushl	$0
	pushl	$113
	jmp	cmnint

	.align	4
	.globl	ivctM5S2
ivctM5S2:
	pushl	$0
	pushl	$114
	jmp	cmnint

	.align	4
	.globl	ivctM5S3
ivctM5S3:
	pushl	$0
	pushl	$115
	jmp	cmnint

	.align	4
	.globl	ivctM5S4
ivctM5S4:
	pushl	$0
	pushl	$116
	jmp	cmnint

	.align	4
	.globl	ivctM5S5
ivctM5S5:
	pushl	$0
	pushl	$117
	jmp	cmnint

	.align	4
	.globl	ivctM5S6
ivctM5S6:
	pushl	$0
	pushl	$118
	jmp	cmnint

	.align	4
	.globl	ivctM5S7
ivctM5S7:
	pushl	$0
	pushl	$119
	jmp	cmnint

	.align	4
	.globl	ivctM6S0
ivctM6S0:
	pushl	$0
	pushl	$120
	jmp	cmnint

	.align	4
	.globl	ivctM6S1
ivctM6S1:
	pushl	$0
	pushl	$121
	jmp	cmnint

	.align	4
	.globl	ivctM6S2
ivctM6S2:
	pushl	$0
	pushl	$122
	jmp	cmnint

	.align	4
	.globl	ivctM6S3
ivctM6S3:
	pushl	$0
	pushl	$123
	jmp	cmnint

	.align	4
	.globl	ivctM6S4
ivctM6S4:
	pushl	$0
	pushl	$124
	jmp	cmnint

	.align	4
	.globl	ivctM6S5
ivctM6S5:
	pushl	$0
	pushl	$125
	jmp	cmnint

	.align	4
	.globl	ivctM6S6
ivctM6S6:
	pushl	$0
	pushl	$126
	jmp	cmnint

	.align	4
	.globl	ivctM6S7
ivctM6S7:
	pushl	$0
	pushl	$127
	jmp	cmnint

	.align	4
	.globl	ivctM7S0
ivctM7S0:
	pushl	$0
	pushl	$128
	jmp	cmnint

	.align	4
	.globl	ivctM7S1
ivctM7S1:
	pushl	$0
	pushl	$129
	jmp	cmnint

	.align	4
	.globl	ivctM7S2
ivctM7S2:
	pushl	$0
	pushl	$130
	jmp	cmnint

	.align	4
	.globl	ivctM7S3
ivctM7S3:
	pushl	$0
	pushl	$131
	jmp	cmnint

	.align	4
	.globl	ivctM7S4
ivctM7S4:
	pushl	$0
	pushl	$132
	jmp	cmnint

	.align	4
	.globl	ivctM7S5
ivctM7S5:
	pushl	$0
	pushl	$133
	jmp	cmnint

	.align	4
	.globl	ivctM7S6
ivctM7S6:
	pushl	$0
	pushl	$134
	jmp	cmnint

	.align	4
	.globl	ivctM7S7
ivctM7S7:
	pushl	$0
	pushl	$135
	jmp	cmnint


	.align	4
	.globl	invaltrap
invaltrap:
	pushl	$0
	pushl	$255
	jmp	cmnint

#endif	/* lint */
