/*
 * Copyright (c) 1989-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__tbl_fdq.c	1.8	96/12/06 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "base_conversion.h"

/*
 * A table for converting a short unsigned u in the range [0,9992] to its
 * equivalent four-character ascii string.   To save space, only multiples of
 * eight are listed; u & 7 must be added in to the result and any carries
 * properly propagated.
 */

const __four_digits_quick_string __four_digits_quick_table[1250] = {
	"0000", "0008", "0016", "0024", "0032", "0040", "0048", "0056",
	"0064", "0072", "0080", "0088", "0096", "0104", "0112", "0120",
	"0128", "0136", "0144", "0152", "0160", "0168", "0176", "0184",
	"0192", "0200", "0208", "0216", "0224", "0232", "0240", "0248",
	"0256", "0264", "0272", "0280", "0288", "0296", "0304", "0312",
	"0320", "0328", "0336", "0344", "0352", "0360", "0368", "0376",
	"0384", "0392", "0400", "0408", "0416", "0424", "0432", "0440",
	"0448", "0456", "0464", "0472", "0480", "0488", "0496", "0504",
	"0512", "0520", "0528", "0536", "0544", "0552", "0560", "0568",
	"0576", "0584", "0592", "0600", "0608", "0616", "0624", "0632",
	"0640", "0648", "0656", "0664", "0672", "0680", "0688", "0696",
	"0704", "0712", "0720", "0728", "0736", "0744", "0752", "0760",
	"0768", "0776", "0784", "0792", "0800", "0808", "0816", "0824",
	"0832", "0840", "0848", "0856", "0864", "0872", "0880", "0888",
	"0896", "0904", "0912", "0920", "0928", "0936", "0944", "0952",
	"0960", "0968", "0976", "0984", "0992", "1000", "1008", "1016",
	"1024", "1032", "1040", "1048", "1056", "1064", "1072", "1080",
	"1088", "1096", "1104", "1112", "1120", "1128", "1136", "1144",
	"1152", "1160", "1168", "1176", "1184", "1192", "1200", "1208",
	"1216", "1224", "1232", "1240", "1248", "1256", "1264", "1272",
	"1280", "1288", "1296", "1304", "1312", "1320", "1328", "1336",
	"1344", "1352", "1360", "1368", "1376", "1384", "1392", "1400",
	"1408", "1416", "1424", "1432", "1440", "1448", "1456", "1464",
	"1472", "1480", "1488", "1496", "1504", "1512", "1520", "1528",
	"1536", "1544", "1552", "1560", "1568", "1576", "1584", "1592",
	"1600", "1608", "1616", "1624", "1632", "1640", "1648", "1656",
	"1664", "1672", "1680", "1688", "1696", "1704", "1712", "1720",
	"1728", "1736", "1744", "1752", "1760", "1768", "1776", "1784",
	"1792", "1800", "1808", "1816", "1824", "1832", "1840", "1848",
	"1856", "1864", "1872", "1880", "1888", "1896", "1904", "1912",
	"1920", "1928", "1936", "1944", "1952", "1960", "1968", "1976",
	"1984", "1992", "2000", "2008", "2016", "2024", "2032", "2040",
	"2048", "2056", "2064", "2072", "2080", "2088", "2096", "2104",
	"2112", "2120", "2128", "2136", "2144", "2152", "2160", "2168",
	"2176", "2184", "2192", "2200", "2208", "2216", "2224", "2232",
	"2240", "2248", "2256", "2264", "2272", "2280", "2288", "2296",
	"2304", "2312", "2320", "2328", "2336", "2344", "2352", "2360",
	"2368", "2376", "2384", "2392", "2400", "2408", "2416", "2424",
	"2432", "2440", "2448", "2456", "2464", "2472", "2480", "2488",
	"2496", "2504", "2512", "2520", "2528", "2536", "2544", "2552",
	"2560", "2568", "2576", "2584", "2592", "2600", "2608", "2616",
	"2624", "2632", "2640", "2648", "2656", "2664", "2672", "2680",
	"2688", "2696", "2704", "2712", "2720", "2728", "2736", "2744",
	"2752", "2760", "2768", "2776", "2784", "2792", "2800", "2808",
	"2816", "2824", "2832", "2840", "2848", "2856", "2864", "2872",
	"2880", "2888", "2896", "2904", "2912", "2920", "2928", "2936",
	"2944", "2952", "2960", "2968", "2976", "2984", "2992", "3000",
	"3008", "3016", "3024", "3032", "3040", "3048", "3056", "3064",
	"3072", "3080", "3088", "3096", "3104", "3112", "3120", "3128",
	"3136", "3144", "3152", "3160", "3168", "3176", "3184", "3192",
	"3200", "3208", "3216", "3224", "3232", "3240", "3248", "3256",
	"3264", "3272", "3280", "3288", "3296", "3304", "3312", "3320",
	"3328", "3336", "3344", "3352", "3360", "3368", "3376", "3384",
	"3392", "3400", "3408", "3416", "3424", "3432", "3440", "3448",
	"3456", "3464", "3472", "3480", "3488", "3496", "3504", "3512",
	"3520", "3528", "3536", "3544", "3552", "3560", "3568", "3576",
	"3584", "3592", "3600", "3608", "3616", "3624", "3632", "3640",
	"3648", "3656", "3664", "3672", "3680", "3688", "3696", "3704",
	"3712", "3720", "3728", "3736", "3744", "3752", "3760", "3768",
	"3776", "3784", "3792", "3800", "3808", "3816", "3824", "3832",
	"3840", "3848", "3856", "3864", "3872", "3880", "3888", "3896",
	"3904", "3912", "3920", "3928", "3936", "3944", "3952", "3960",
	"3968", "3976", "3984", "3992", "4000", "4008", "4016", "4024",
	"4032", "4040", "4048", "4056", "4064", "4072", "4080", "4088",
	"4096", "4104", "4112", "4120", "4128", "4136", "4144", "4152",
	"4160", "4168", "4176", "4184", "4192", "4200", "4208", "4216",
	"4224", "4232", "4240", "4248", "4256", "4264", "4272", "4280",
	"4288", "4296", "4304", "4312", "4320", "4328", "4336", "4344",
	"4352", "4360", "4368", "4376", "4384", "4392", "4400", "4408",
	"4416", "4424", "4432", "4440", "4448", "4456", "4464", "4472",
	"4480", "4488", "4496", "4504", "4512", "4520", "4528", "4536",
	"4544", "4552", "4560", "4568", "4576", "4584", "4592", "4600",
	"4608", "4616", "4624", "4632", "4640", "4648", "4656", "4664",
	"4672", "4680", "4688", "4696", "4704", "4712", "4720", "4728",
	"4736", "4744", "4752", "4760", "4768", "4776", "4784", "4792",
	"4800", "4808", "4816", "4824", "4832", "4840", "4848", "4856",
	"4864", "4872", "4880", "4888", "4896", "4904", "4912", "4920",
	"4928", "4936", "4944", "4952", "4960", "4968", "4976", "4984",
	"4992", "5000", "5008", "5016", "5024", "5032", "5040", "5048",
	"5056", "5064", "5072", "5080", "5088", "5096", "5104", "5112",
	"5120", "5128", "5136", "5144", "5152", "5160", "5168", "5176",
	"5184", "5192", "5200", "5208", "5216", "5224", "5232", "5240",
	"5248", "5256", "5264", "5272", "5280", "5288", "5296", "5304",
	"5312", "5320", "5328", "5336", "5344", "5352", "5360", "5368",
	"5376", "5384", "5392", "5400", "5408", "5416", "5424", "5432",
	"5440", "5448", "5456", "5464", "5472", "5480", "5488", "5496",
	"5504", "5512", "5520", "5528", "5536", "5544", "5552", "5560",
	"5568", "5576", "5584", "5592", "5600", "5608", "5616", "5624",
	"5632", "5640", "5648", "5656", "5664", "5672", "5680", "5688",
	"5696", "5704", "5712", "5720", "5728", "5736", "5744", "5752",
	"5760", "5768", "5776", "5784", "5792", "5800", "5808", "5816",
	"5824", "5832", "5840", "5848", "5856", "5864", "5872", "5880",
	"5888", "5896", "5904", "5912", "5920", "5928", "5936", "5944",
	"5952", "5960", "5968", "5976", "5984", "5992", "6000", "6008",
	"6016", "6024", "6032", "6040", "6048", "6056", "6064", "6072",
	"6080", "6088", "6096", "6104", "6112", "6120", "6128", "6136",
	"6144", "6152", "6160", "6168", "6176", "6184", "6192", "6200",
	"6208", "6216", "6224", "6232", "6240", "6248", "6256", "6264",
	"6272", "6280", "6288", "6296", "6304", "6312", "6320", "6328",
	"6336", "6344", "6352", "6360", "6368", "6376", "6384", "6392",
	"6400", "6408", "6416", "6424", "6432", "6440", "6448", "6456",
	"6464", "6472", "6480", "6488", "6496", "6504", "6512", "6520",
	"6528", "6536", "6544", "6552", "6560", "6568", "6576", "6584",
	"6592", "6600", "6608", "6616", "6624", "6632", "6640", "6648",
	"6656", "6664", "6672", "6680", "6688", "6696", "6704", "6712",
	"6720", "6728", "6736", "6744", "6752", "6760", "6768", "6776",
	"6784", "6792", "6800", "6808", "6816", "6824", "6832", "6840",
	"6848", "6856", "6864", "6872", "6880", "6888", "6896", "6904",
	"6912", "6920", "6928", "6936", "6944", "6952", "6960", "6968",
	"6976", "6984", "6992", "7000", "7008", "7016", "7024", "7032",
	"7040", "7048", "7056", "7064", "7072", "7080", "7088", "7096",
	"7104", "7112", "7120", "7128", "7136", "7144", "7152", "7160",
	"7168", "7176", "7184", "7192", "7200", "7208", "7216", "7224",
	"7232", "7240", "7248", "7256", "7264", "7272", "7280", "7288",
	"7296", "7304", "7312", "7320", "7328", "7336", "7344", "7352",
	"7360", "7368", "7376", "7384", "7392", "7400", "7408", "7416",
	"7424", "7432", "7440", "7448", "7456", "7464", "7472", "7480",
	"7488", "7496", "7504", "7512", "7520", "7528", "7536", "7544",
	"7552", "7560", "7568", "7576", "7584", "7592", "7600", "7608",
	"7616", "7624", "7632", "7640", "7648", "7656", "7664", "7672",
	"7680", "7688", "7696", "7704", "7712", "7720", "7728", "7736",
	"7744", "7752", "7760", "7768", "7776", "7784", "7792", "7800",
	"7808", "7816", "7824", "7832", "7840", "7848", "7856", "7864",
	"7872", "7880", "7888", "7896", "7904", "7912", "7920", "7928",
	"7936", "7944", "7952", "7960", "7968", "7976", "7984", "7992",
	"8000", "8008", "8016", "8024", "8032", "8040", "8048", "8056",
	"8064", "8072", "8080", "8088", "8096", "8104", "8112", "8120",
	"8128", "8136", "8144", "8152", "8160", "8168", "8176", "8184",
	"8192", "8200", "8208", "8216", "8224", "8232", "8240", "8248",
	"8256", "8264", "8272", "8280", "8288", "8296", "8304", "8312",
	"8320", "8328", "8336", "8344", "8352", "8360", "8368", "8376",
	"8384", "8392", "8400", "8408", "8416", "8424", "8432", "8440",
	"8448", "8456", "8464", "8472", "8480", "8488", "8496", "8504",
	"8512", "8520", "8528", "8536", "8544", "8552", "8560", "8568",
	"8576", "8584", "8592", "8600", "8608", "8616", "8624", "8632",
	"8640", "8648", "8656", "8664", "8672", "8680", "8688", "8696",
	"8704", "8712", "8720", "8728", "8736", "8744", "8752", "8760",
	"8768", "8776", "8784", "8792", "8800", "8808", "8816", "8824",
	"8832", "8840", "8848", "8856", "8864", "8872", "8880", "8888",
	"8896", "8904", "8912", "8920", "8928", "8936", "8944", "8952",
	"8960", "8968", "8976", "8984", "8992", "9000", "9008", "9016",
	"9024", "9032", "9040", "9048", "9056", "9064", "9072", "9080",
	"9088", "9096", "9104", "9112", "9120", "9128", "9136", "9144",
	"9152", "9160", "9168", "9176", "9184", "9192", "9200", "9208",
	"9216", "9224", "9232", "9240", "9248", "9256", "9264", "9272",
	"9280", "9288", "9296", "9304", "9312", "9320", "9328", "9336",
	"9344", "9352", "9360", "9368", "9376", "9384", "9392", "9400",
	"9408", "9416", "9424", "9432", "9440", "9448", "9456", "9464",
	"9472", "9480", "9488", "9496", "9504", "9512", "9520", "9528",
	"9536", "9544", "9552", "9560", "9568", "9576", "9584", "9592",
	"9600", "9608", "9616", "9624", "9632", "9640", "9648", "9656",
	"9664", "9672", "9680", "9688", "9696", "9704", "9712", "9720",
	"9728", "9736", "9744", "9752", "9760", "9768", "9776", "9784",
	"9792", "9800", "9808", "9816", "9824", "9832", "9840", "9848",
	"9856", "9864", "9872", "9880", "9888", "9896", "9904", "9912",
	"9920", "9928", "9936", "9944", "9952", "9960", "9968", "9976",
	"9984", "9992"};


#ifdef TEST
void
__four_digits_quick(u, s)
	short unsigned  u;
	char	*s;

/* Convert u to four digits at *s	 which may not be aligned. */

{
	char	carry;
	char	*pt, *ps;

	pt = ((char *) &(__four_quick_table[u >> 3])) + 3;
	ps = s + 3;
	carry = (u & 7) + *pt--;

	while (carry > '9') {
		*ps-- = carry - 10;
		carry = *pt-- + 1;
	};
	*ps-- = carry;
	while (ps >= s) {
		*ps-- = *pt--;
	};
}

main()
{
	/* Test __four_digits_quick	 */
	short unsigned  u;
	int		i, t;
	char		s[5];

	for (i = 0; i <= 10000; i++) {
		u = i;
		__four_digits_quick(u, s);
		s[5] = 0;
		sscanf(s, "%d", &t);
		if (t != u)
			printf(" u %d s %s t %d \n", u, s, t);
	}
}
#endif TEST
