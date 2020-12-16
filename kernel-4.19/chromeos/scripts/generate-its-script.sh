#!/bin/bash

# Generate an its script used to build a u-boot fitImage kernel binary
# that contains the kernel as well as device trees used when booting it
# via mkimage or dtc.

# Using mkimage will insert extra hashes that dtc will not add.

kernel_arch=${ARCH}
its_out=/dev/stdout
dtb_dir=.
compression="none"

show_usage()
{
cat <<-EOF
$0 [options] <image> <dtbs>
	-a <arch>          CPU architecture
	-c <compression>   Compression to use (none|lzma|lz4)
	-d <dtb_dir>       directory where dtb files exist
	-h                 shows this message
	-o <its_script>    output its script pathname
	-r <ramdisk_path>  ramdisk image to include

$0 Image dts/*.dtb | dtc -I dts -O dtb -p 1024 > Image.fit
EOF
}

die()
{
	exit 1
}

OPTIND=1
while getopts "a:c:d:ho:r:" opt; do
	case "$opt" in
		a)
			kernel_arch=${OPTARG}
			;;
		c)
			compression=${OPTARG}
			;;
		d)
			dtb_dir=${OPTARG}
			;;
		h)
			show_usage
			exit 0
			;;
		o)
			its_out=${OPTARG}
			;;
		r)
			ramdisk_path=${OPTARG}
			;;
	esac
done

shift $((OPTIND-1))

if [ -z "${kernel_arch}" ]; then
	>&2 echo "ERROR: CPU architecture not specified"
	show_usage
	die
fi

image_path=${1}
shift

if [[ "${compression}" == "lzma" ]]; then
	lzma -9 -z -f -k "${image_path}" || die
	image_path="${image_path}.lzma"
elif [[ "${compression}" == "lz4" ]]; then
	lz4 -20 -z -f "${image_path}" "${image_path}.lz4" || die
	image_path="${image_path}.lz4"
elif [[ "${compression}" != "none" ]]; then
	>&2 echo "ERROR: compression method unknown"
	show_usage
	die
fi

cat > "${its_out}" <<-EOF || die
/dts-v1/;

/ {
	description = "Chrome OS kernel image with one or more FDT blobs";
	#address-cells = <1>;

	images {
		kernel@1 {
			data = /incbin/("${image_path}");
			type = "kernel_noload";
			arch = "${kernel_arch}";
			os = "linux";
			compression = "${compression}";
			load = <0>;
			entry = <0>;
		};
EOF

if [ -n "${ramdisk_path}" ]; then
	cat >> "${its_out}" <<-EOF || die
		ramdisk@1 {
			data = /incbin/("${ramdisk_path}");
			type = "ramdisk";
			arch = "${kernel_arch}";
			os = "linux";
			compression = "gzip";
			load = <0>;
			entry = <0>;
		};
	EOF
fi

iter=1
for dtb in "$@" ; do
	cat >> "${its_out}" <<-EOF || die
		fdt@${iter} {
			description = "$(basename ${dtb})";
			data = /incbin/("${dtb_dir}/${dtb}");
			type = "flat_dt";
			arch = "${kernel_arch}";
			compression = "none";
			hash@1 {
				algo = "sha1";
			};
		};
	EOF
	((++iter))
done

cat <<-EOF >>"${its_out}"
	};
	configurations {
		default = "conf@1";
EOF

for i in $(seq 1 $((iter-1))) ; do
	cat >> "${its_out}" <<-EOF || die
		conf@${i} {
			kernel = "kernel@1";
	EOF
	if [ -n "${ramdisk_path}" ]; then
		cat >> "${its_out}" <<-EOF || die
			ramdisk = "ramdisk@1";
		EOF
	fi
	cat >> "${its_out}" <<-EOF || die
			fdt = "fdt@${i}";
		};
	EOF
done

echo "};" >> "${its_out}"
echo "};" >> "${its_out}"
