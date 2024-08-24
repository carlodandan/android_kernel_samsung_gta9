# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2020 Mediatek Inc.

#!/usr/bin/env python

from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
import os
import sys
import re

# Parse project defconfig to get special config file, build config file, and out-of-tree kernel modules
def get_config_in_defconfig(file_name, kernel_dir):
    with open(file_name, 'r') as file_handle:
        pattern_cficlang = re.compile(r'^CONFIG_CFI_CLANG\s*=\s*(.+)$')
        pattern_buildconfig = re.compile(r'^CONFIG_BUILD_CONFIG_FILE\s*=\s*(.+)$')
        pattern_extmodules = re.compile(r'^CONFIG_EXT_MODULES\s*=\s*(.+)$')
        special_defconfig = ''
        build_config = ''
        ext_modules = ''
        for line in file_handle:
            result = pattern_cficlang.match(line)
            if not result:
                special_defconfig = "gki_defconfig"
            result = pattern_buildconfig.match(line)
            if result:
                build_config = result.group(1).strip('"')
            result = pattern_extmodules.match(line)
            if result:
                ext_modules = result.group(1).strip('"')
    return (special_defconfig, build_config, ext_modules)

def help():
    print('Usage:')
    print('  python scripts/gen_build_config.py --project <project> --kernel-defconfig <kernel project defconfig file> --kernel-defconfig-overlays <kernel project overlay defconfig files> --kernel-build-config-overlays <kernel build config overlays> --build-mode <mode> --out-file <gen build.config>')
    print('Or:')
    print('  python scripts/gen_build_config.py -p <project> --kernel-defconfig <kernel project defconfig file> --kernel-defconfig-overlays <kernel project overlay defconfig files> --kernel-build-config-overlays <kernel build config overlays> -m <mode> -o <gen build.config>')
    print('')
    print('Attention: Must set generated build.config, and project or kernel project defconfig file!!')
    sys.exit(2)

def main(**args):

    project = args["project"]
    kernel_defconfig = args["kernel_defconfig"]
    kernel_defconfig_overlays = args["kernel_defconfig_overlays"]
    kernel_build_config_overlays = args["kernel_build_config_overlays"]
    build_mode = args["build_mode"]
    abi_mode = args["abi_mode"]
    out_file = args["out_file"]
    if ((project == '') and (kernel_defconfig == '')) or (out_file == ''):
        help()

    current_file_path = os.path.abspath(sys.argv[0])
    abs_kernel_dir = os.path.dirname(os.path.dirname(current_file_path))

    gen_build_config = out_file
    gen_build_config_dir = os.path.dirname(gen_build_config)
    if not os.path.exists(gen_build_config_dir):
        os.makedirs(gen_build_config_dir)

    mode_config = ''
    if (build_mode == 'eng') or (build_mode == 'userdebug'):
        mode_config = '%s.config' % (build_mode)
    project_defconfig_name = ''
    if kernel_defconfig:
        project_defconfig_name = kernel_defconfig
        pattern_project = re.compile(r'^(.+)_defconfig$')
        project = pattern_project.match(kernel_defconfig).group(1).strip()
    else:
        project_defconfig_name = '%s_defconfig' % (project)
    defconfig_dir = ''
    if os.path.exists('%s/arch/arm/configs/%s' % (abs_kernel_dir, project_defconfig_name)):
        defconfig_dir = 'arch/arm/configs'
    elif os.path.exists('%s/arch/arm64/configs/%s' % (abs_kernel_dir, project_defconfig_name)):
        defconfig_dir = 'arch/arm64/configs'
    else:
        print('Error: cannot find project defconfig file under ' + abs_kernel_dir)
        sys.exit(2)
    project_defconfig = '%s/%s/%s' % (abs_kernel_dir, defconfig_dir, project_defconfig_name)

    special_defconfig = ''
    build_config = ''
    ext_modules = ''
    kernel_dir = ''
    if project_defconfig_name == 'gki_defconfig':
        build_config = 'build.config.mtk.aarch64'
        mode_config = ''
    else:
        (special_defconfig, build_config, ext_modules) = get_config_in_defconfig(project_defconfig, os.path.basename(abs_kernel_dir))
    build_config = '%s/%s' % (abs_kernel_dir, build_config)
    file_text = []
    if os.path.exists(build_config):
        with open(build_config, 'r') as file_handle:
            for line in file_handle:
                line_strip = line.strip()
                pattern_cc = re.compile(r'^CC\s*=\s*(.+)$')
                result = pattern_cc.match(line_strip)
                if result:
                    line_strip = 'CC="${CC_WRAPPER} %s"' % (result.group(1).strip())
                line_strip = line_strip.replace("$$","$")
                file_text.append(line_strip)
                pattern_kernel_dir = re.compile(r'^KERNEL_DIR\s*=\s*(.+)$')
                result = pattern_kernel_dir.match(line_strip)
                if result:
                    kernel_dir = result.group(1).strip()
    else:
        print('Error: cannot get build.config under ' + abs_kernel_dir + '.')
        print('Please check whether ' + project_defconfig + ' defined CONFIG_BUILD_CONFIG_FILE.')
        sys.exit(2)

    file_text.append("PATH=${ROOT_DIR}/../prebuilts/perl/linux-x86/bin:${ROOT_DIR}/build/build-tools/path/linux-x86:/usr/bin:/bin")
    file_text.append("MAKE_GOALS=\"all\"")
    if (build_mode != 'user'):
        file_text.append("TRIM_NONLISTED_KMI=")
        file_text.append("KMI_SYMBOL_LIST_STRICT_MODE=")
    file_text.append("MODULES_ORDER=")
    file_text.append("KMI_ENFORCED=1")
    file_text.append("if [ \"x${DO_ABI_MONITOR}\" == \"x1\" ]; then")
    file_text.append("  KMI_SYMBOL_LIST_MODULE_GROUPING=0")
    file_text.append("  KMI_SYMBOL_LIST_ADD_ONLY=1")
    file_text.append("  ADDITIONAL_KMI_SYMBOL_LISTS=\"${ADDITIONAL_KMI_SYMBOL_LISTS} android/abi_gki_aarch64\"")
    file_text.append("fi")
    file_text.append("unset BUILD_NUMBER")

    all_defconfig = ''
    pre_defconfig_cmds = ''
    if not special_defconfig:
        all_defconfig = '%s %s %s' % (project_defconfig_name, kernel_defconfig_overlays, mode_config)
    else:
        rel_kernel_path = 'REL_KERNEL_PATH=`./${KERNEL_DIR}/scripts/get_rel_path.sh ${ROOT_DIR} %s`' % (kernel_dir)
        file_text.append(rel_kernel_path)
        all_defconfig = '%s ../../../${REL_KERNEL_PATH}/${OUT_DIR}/%s.config %s %s' % (special_defconfig, project, kernel_defconfig_overlays, mode_config)
        pre_defconfig_cmds = 'PRE_DEFCONFIG_CMDS=\"cp -p ${KERNEL_DIR}/%s/%s ${OUT_DIR}/%s.config\"' % (defconfig_dir, project_defconfig_name, project)
    all_defconfig = 'DEFCONFIG=\"%s\"' % (all_defconfig.strip())
    file_text.append(all_defconfig)
    if pre_defconfig_cmds:
        file_text.append(pre_defconfig_cmds)

    ext_modules_list = ''
    ext_modules_file = '%s/kernel/configs/ext_modules.list' % (abs_kernel_dir)
    if os.path.exists(ext_modules_file):
        with open(ext_modules_file, 'r') as file_handle:
            for line in file_handle:
                line_strip = line.strip()
                ext_modules_list = '%s %s' % (ext_modules_list, line_strip)
    ext_modules_list = 'EXT_MODULES=\"%s %s\"' % (ext_modules_list.strip(), ext_modules.strip())
    file_text.append(ext_modules_list)

    file_text.append("DIST_CMDS='cp -p ${OUT_DIR}/.config ${DIST_DIR}'")

    gen_build_config_mtk = '%s.mtk' % (gen_build_config)
    with open(gen_build_config_mtk, 'w') as file_handle:
        for line in file_text:
            file_handle.write(line + '\n')

    gki_build_config = '%s/build.config.gki.aarch64' % (abs_kernel_dir)
    gen_build_config_gki = '%s/build.config.gki.aarch64' % (gen_build_config_dir)
    file_text = []
    if os.path.exists(gki_build_config):
        with open(gki_build_config, 'r') as file_handle:
            for line in file_handle:
                line_strip = line.strip()
                pattern_source = re.compile(r'^\.\s.+$')
                result = pattern_source.match(line_strip)
                if not result:
                    file_text.append(line_strip)

    with open(gen_build_config_gki, 'w') as file_handle:
        for line in file_text:
            file_handle.write(line + '\n')
        file_handle.write('unset BUILD_CONFIG\n')

if __name__ == '__main__':
    parser = ArgumentParser(description="Generate build configuration script",
                            formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('--project', '-p', default="", help="Project name")
    parser.add_argument('--kernel-defconfig', default="", help="Kernel project defconfig file")
    parser.add_argument('--kernel-defconfig-overlays', default="", help="Kernel project overlay defconfig files")
    parser.add_argument('--kernel-build-config-overlays', default="", help="Kernel build config overlays")
    parser.add_argument('--build-mode', '-m', default="user", choices=['user', 'eng', 'userdebug'], help="Build mode")
    parser.add_argument('--abi-mode', default="", help="ABI mode")
    parser.add_argument('--out-file', '-o', default="", help="Generated build configuration file")

    args = vars(parser.parse_args())

    main(**args)
