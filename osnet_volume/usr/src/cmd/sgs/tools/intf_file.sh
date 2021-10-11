#! /bin/sh
#
#ident	"@(#)intf_file.sh	1.4	94/09/14 SMI"
#
# This script displays a files `physical binary interface'.
# The "physical" binary interface is a description of what the linker will
# actually let a program bind to.  It's distinct from the "logical" binary
# interface in that the latter is a description of what you're "supposed"
# to use, whereas the former is what a misbehaved application *could* use.

if [ ! -f $1 ]; then
	echo "$0: $1 - not a file"
	exit 1
fi
echo "Interface record for $1:"
echo ""
nm -n $1 | nawk -F\| '
	BEGIN {
		di = 0;			# Initialize indices
		fi = 0;
		ii = 0;
		dl = 0;			# Initialize maximum symbol length
		fl = 0;
		il = 0;
	}
	$7 ~ /^UNDEF/ {
		im[ii++] = $8;
		if (length($8) > il)
			il = length($8);
		next
	}
	$5 ~ /(^GLOB|^WEAK)/ && $4 ~ /^FUNC/ {
		fu[fi++] = $8;
		if (length($8) > fl)
			fl = length($8);

	}
	$8 ~ /(^_DYNAMIC$|^_GLOBAL_OFFSET_TABLE_$|^_etext$|^_edata$|^_end$)/ {
		next;
	}
	$5 ~ /(^GLOB|^WEAK)/ && $4 ~ /^OBJT/ {
		ds[di] = $3;
		da[di++] = $8;
		if (length($8) > dl)
			dl = length($8);
	}
	END {
		printf("Exported functions (%d):\n", fi);

		# Calculate values for multi-column printing
		#	c is number of columns
		#	r is number of rows
		#	w is with of a column?

		c = int(64 / (fl + 1));
		if (c == 0) {
			c=1
			w=70
			r=fi
		} else {
			w = int(70 / c);
			r = int((fi + c - 1) / c);
		}

		for (i = 0; i < r ; i++) {
			printf("\t");
			for (j = 0; j < c; j++)
				if ((k = i + (j * r)) < fi) {
					printf("%s", fu[k]);
					if (j != (c - 1))
						printf("%-*s",
						    length(fu[k]) - w + 1, "");
				}
			printf("\n");
		}
		printf("\nExported data[size] (%d):\n", di);
		c = int(64 / (dl + 1 + 10));
		if (c == 0) {
			c=1
			w=70
			r=di

		} else {
			w = int(70 / c);
			r = int((di + c - 1) / c);
		}
		for (i = 0; i < r ; i++) {
			printf("\t");
			for (j = 0; j < c; j++)
				if ((k = i + (j * r)) < di) {
					printf("%s[%x]", da[k], ds[k]);
					if (j != (c - 1))
						printf("%-*s",
						    w - length(da[k]) - 10, "");
				}
			printf("\n");
		}
		printf("\nImported symbols (%d):\n", ii);
		if (ii != 0) {
			c = int(64 / il);
			if (c == 0) {
				c=1
				w=70
				r=ii

			} else {
				w = int(70 / c);
				r = int((ii + c - 1) / c);
			}

			for (i = 0; i < r ; i++) {
				printf("\t");
				for (j = 0; j < c; j++)
					if ((k = i + (j * r)) < ii) {
						printf("%s", im[k]);
						if (j != (c - 1))
							printf("%-*s",
						    	length(im[k]) - w + 1, "");
					}
				printf("\n");
			}
		}
	}'
