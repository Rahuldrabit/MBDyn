#!/bin/bash -f

# MBDyn (C) is a multibody analysis code.
# http://www.mbdyn.org
#
# Copyright (C) 1996-2023
#
# Pierangelo Masarati   <pierangelo.masarati@polimi.it>
# Paolo Mantegazza      <paolo.mantegazza@polimi.it>
#
# Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
# via La Masa, 34 - 20156 Milano, Italy
# http://www.aero.polimi.it
#
# Changing this copyright notice is forbidden.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation (version 2 of the License).
#
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# AUTHOR: Reinhard Resch <mbdyn-user@a1.net>
# Copyright (C) 2023(-2023) all rights reserved.

# The copyright of this code is transferred
# to Pierangelo Masarati and Paolo Mantegazza
# for use in the software MBDyn as described
# in the GNU Public License version 2.1

## Use simple_testsuite.sh in order to check if a model can run or not

set -o pipefail ## Needed for commands like "mbdyn -f input_file |& tee logfile"

program_name="$0"
mbdyn_testsuite_timeout="unlimited"
mbdyn_testsuite_prefix_output=""
mbdyn_testsuite_prefix_input=""
mbdyn_input_filter=""
mbdyn_verbose_output="no"
mbdyn_keep_output="unexpected"
mbdyn_print_res="no"
mbdyn_patch_input="no"
mbdyn_abort_after_step=""
mbdyn_exec_gen="yes"
mbdyn_exec_solver="yes"
mbdyn_enable_gtest="yes"
update_reference_test_status="no"
use_reference_test_status="no"
skip_expected_failures="no"

declare -i mbdyn_exclude_inverse_dynamics=0
declare -i mbdyn_exclude_initial_value=0
mbdyn_suppressed_errors=""

program_dir=$(realpath $(dirname "${program_name}"))

declare -i mbd_exit_status_mask=0 ## Define the errors codes which should not cause the pipeline to fail
MBDYN_EXEC="${MBDYN_EXEC:-mbdyn}"
MBDYN_ARGS_ADD="${MBDYN_ARGS_ADD:--CF}"
OCT_PKG_INSTALL_PREFIX="${OCT_PKG_INSTALL_PREFIX:-${program_dir}/var/cache/share/octave}"
if ! test -z "${OCT_PKG_INSTALL_PREFIX}"; then
    OCTAVE_LOCAL_LIST=`printf '%s;' "${OCT_PKG_INSTALL_PREFIX}/octave_packages"`
else
    OCTAVE_LOCAL_LIST=""
fi
OCTAVE_EXEC="${OCTAVE_EXEC:-octave}"
temp_octaverc=$(mktemp "${TMPDIR:-/tmp/}$(basename $0).XXXXXXXXXXXX")
OCTAVE_LOCAL_LIST=$OCTAVE_LOCAL_LIST awk 'BEGIN {DONE=0} {if ($4=="%CI_LOCAL_LIST") {print "pkg local_list ",ENVIRON["OCTAVE_LOCAL_LIST"]," %CI_LOCAL_LIST"; DONE=1} else {print $0}} END {if (DONE==0) {print "pkg local_list ",ENVIRON["OCTAVE_LOCAL_LIST"]," %CI_LOCAL_LIST";}}' $HOME/.octaverc > $temp_octaverc
mv $temp_octaverc $HOME/.octaverc
PYTHON_EXEC="${PYTHON_EXEC:-python3}"
TESTSUITE_TIME_CMD="${TESTSUITE_TIME_CMD:-/usr/bin/time --verbose}"
JUNIT_XML_KEEP_ALL_OUTPUT="${JUNIT_XML_KEEP_ALL_OUTPUT:-none}"


if ! test -f "${program_dir}/mbdyn_input_file_format.awk"; then
    program_dir=$(realpath $(which "${program_name}"))
fi

if test -f "${program_dir}/mbdyn_input_file_format.awk"; then
    export AWKPATH=${program_dir}:${AWKPATH}
    mbdyn_sed_prefix=${program_dir}/
else
    mbdyn_sed_prefix=""
fi

mbdyn_patch_input_sed_expression="${mbdyn_sed_prefix}mbdyn_testsuite_patch.sed"
mbdyn_patch_input_sed_args='-E -f'
## Disable multithreaded BLAS by default
export OMP_NUM_THREADS=1
export MKL_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export PYTHONPATH="${PYTHONPATH}:${program_dir}/libraries/libmbc"
## Might be used for Octave scripts (e.g. via mboct-mbdyn-pkg)

MBD_NUM_TASKS=${MBD_NUM_TASKS:-$(( $(lscpu | awk '/^Socket\(s\)/{ print $2 }') * $(lscpu | awk '/^Core\(s\) per socket/{ print $4 }') ))}
MBD_NUM_THREADS=${MBD_NUM_THREADS:-1}

while ! test -z "$1"; do
    case "$1" in
        --prefix-output)
            mbdyn_testsuite_prefix_output="$2"
            shift
            ;;
        --prefix-input)
            mbdyn_testsuite_prefix_input="$2"
            shift
            ;;
        --timeout)
            mbdyn_testsuite_timeout="$2"
            shift
            ;;
        --update-reference-test-status)
            update_reference_test_status="$2"
            if [[ -z "$mbdyn_suppressed_errors" ]]; then
                mbdyn_suppressed_errors="feature"
            else
                mbdyn_suppressed_errors="$mbdyn_suppressed_errors|feature"
            fi
            shift
            ;;
        --use-reference-test-status)
            use_reference_test_status="$2"
            shift
            ;;
        --skip-expected-failures)
            skip_expected_failures="$2"
            shift
            ;;
        --regex-filter-include|--regex-filter-exclude)
            if ! test -z "${mbdyn_input_filter}"; then
                mbdyn_input_filter="${mbdyn_input_filter} -and "
            fi
            case "$1" in
                --regex-filter-include)
                    mbdyn_input_filter="${mbdyn_input_filter} -regex $2"
                    ;;
                --regex-filter-exclude)
                    mbdyn_input_filter="${mbdyn_input_filter} -not -regex $2"
                    ;;
            esac
            shift
            ;;
        --exclude-inverse-dynamics)
            mbdyn_exclude_inverse_dynamics="$2"
            shift
            ;;
        --exclude-initial-value)
            mbdyn_exclude_initial_value="$2"
            shift
            ;;
        --threads)
            MBD_NUM_THREADS="$2"
            shift
            ;;
        --tasks)
            MBD_NUM_TASKS="$2"
            shift
            ;;
        --verbose)
            mbdyn_verbose_output="$2"
            shift
            ;;
        --keep-output)
            mbdyn_keep_output="$2"
            shift
            ;;
        --keep-output-junit-xml)
            JUNIT_XML_KEEP_ALL_OUTPUT="$2"
            shift
            ;;
        --patch-input)
            if ! test -z "${mbdyn_abort_after_step}"; then
                echo "--patch-input must not be used in combination with --abort-after-step"
                exit 1
            fi
            mbdyn_patch_input="$2"
            shift
            ;;
        --abort-after-step)
            if test "${mbdyn_patch_input}" = "yes"; then
                echo "--abort-after must not be used in combination with --patch-input"
                exit 1
            fi
            mbdyn_patch_input="yes"
            mbdyn_abort_after_step="$2"
            mbdyn_patch_input_sed_expression=`printf '/^[[:space:]]*\<end\>:[[:space:]]*initial[[:space:]]*value[[:space:]]*;[[:space:]]*$/i abort after: regular step, %d;' "${mbdyn_abort_after_step}"`
            mbdyn_patch_input_sed_args='-E'
            shift
            ;;
        --mbdyn-exec)
            MBDYN_EXEC="$2"
            shift
            ;;
        --mbdyn-args-add)
            MBDYN_ARGS_ADD="$2"
            shift
            ;;
        --exec-gen)
            mbdyn_exec_gen="$2"
            shift
            ;;
        --exec-solver)
            mbdyn_exec_solver="$2"
            shift
            ;;
        --enable-gtest)
            mbdyn_enable_gtest="$2"
            shift
            ;;
        --help)
            printf "%s\n  --prefix-output <output_dir>\n" "${program_name}"
            printf "  --prefix-input <input_dir>\n"
            printf "  --timeout <timeout_seconds>\n"
            printf "  --regex-filter <input_file_path_filter>\n"
            printf "  --exclude-inverse-dynamics {0|1}\n"
            printf "  --exclude-initial-value {0|1}\n"
            printf "  --threads <number_of_threads_per_task>\n"
            printf "  --tasks <number_of_tasks>\n"
            printf "  --verbose {yes|no}\n"
            printf "  --keep-output {all|failed|unexpected}\n"
            printf "  --patch-input {yes|no}\n"
            printf "  --skip-expected-failures {yes|no}\n"
            printf "  --update-reference-test-status {failed|passed|no|{all|yes}}\n"
            printf "  --use-reference-test-status {yes|no}\n"
            printf "  --mbdyn-exec <mbdyn-binary>\n"
            printf "  --mbdyn-args-add \"<arg1> <arg2> ... <argN>\"\n"
            printf "  --exec-gen {yes|no}\n"
            printf "  --exec-solver {yes|no}\n"
            printf "  --exec-status-mask <mask_errors_to_be_ignored>\n"
            printf "  --print-resources {no|all|time}\n"
            printf "  --suppressed-errors {syntax|element|feature|module|loadable|socked|interrupted|solver}\n"
            printf "  --help\n"
            exit 1;
            ;;
        --exit-status-mask)
            ((mbd_exit_status_mask=$2))
            shift
            ;;
        --print-resources)
            mbdyn_print_res="$2"
            shift
            ;;
        --suppressed-errors)
            if [[ -z "$mbdyn_suppressed_errors" ]]; then
                mbdyn_suppressed_errors="$2"
            else
                mbdyn_suppressed_errors="$mbdyn_suppressed_errors|$2"
            fi
            shift
            ;;
        *)
            printf "%s: invalid argument \"%s\"\n" "${program_name}" "$1"
            exit 1
            ;;
    esac
    shift
done

if test -z "${mbdyn_testsuite_timeout}"; then
    mbdyn_testsuite_timeout="unlimited"
fi

if test -z "${mbdyn_testsuite_prefix_output}"; then
    printf "%s: missing argument --prefix-output <output-dir>\n" "${program_name}"
    exit 1
fi

mbdyn_testsuite_prefix_output="$(realpath ${mbdyn_testsuite_prefix_output})"

if ! test -d "${mbdyn_testsuite_prefix_output}"; then
    if ! mkdir -p "${mbdyn_testsuite_prefix_output}"; then
        echo "Failed to create directory \"${mbdyn_testsuite_prefix_output}\""
        exit 1
    fi
fi

if test -z "${mbdyn_testsuite_prefix_input}"; then
    printf "%s: missing argument --prefix-input <input-dir>\n" "${program_name}"
    exit 1
fi

if ! test -d "${mbdyn_testsuite_prefix_input}"; then
    printf "%s: argument --prefix-input \"%s\" is not valid\n" "${program_name}" "${mbdyn_testsuite_prefix_input}"
    exit 1
fi

if test "${MBD_NUM_TASKS}" -lt 1; then
    printf "%s: argument --tasks \"%s\" is not valid\n" "${program_name}" "${MBD_NUM_TASKS}"
    exit 1
fi

if test "${MBD_NUM_THREADS}" -lt 1; then
    printf "%s: argument --threads \"%s\" is not valid\n" "${program_name}" "${MBD_NUM_THREADS}"
    exit 1
fi

mbdyn_testsuite_prefix_input=`realpath ${mbdyn_testsuite_prefix_input}`

skipped_tests=""
passed_tests=""
failed_tests=""
timeout_tests=""
modules_not_found=""
suppressed_failures=""
unexpected_faults=""
regressions=""
fixed_failures=""
known_failures=""

if ! test -z "${mbdyn_input_filter}"; then
    mbdyn_input_filter="-and ${mbdyn_input_filter}"
fi

declare -i idx_test=0

MBD_INPUT_FILES_FOUND=`find ${mbdyn_testsuite_prefix_input} '(' -type f ${mbdyn_input_filter} -and -not -name '*_patched_*.mbd' ')' -print0 | xargs -0 awk -v exclude_initial_value=$((mbdyn_exclude_initial_value)) -v exclude_inverse_dynamics=$((mbdyn_exclude_inverse_dynamics)) -f mbdyn_input_file_format.awk`

function simple_testsuite_run_test()
{
    mbd_status_file=""
    mbd_filename=""
    mbd_exec_gen_script="yes"
    mbd_exec_run_script="yes"
    mbd_exec_solver="yes"
    declare -i idx_test=-1

    case "${mbdyn_patch_input}" in
        yes)
            mbd_exec_run_script="no"
            ;;
    esac

    while ! test -z "$1"; do
        case "$1" in
            --status)
                mbd_status_file="$2"
                shift
                ;;
            --input)
                mbd_filename="$2"
                shift
                ;;
            --index)
                idx_test="$2"
                shift
                ;;
            --exec-gen)
                mbd_exec_gen_script="$2"
                shift
                ;;
            --exec-run)
                mbd_exec_run_script="$2"
                shift
                ;;
            --exec-solver)
                mbd_exec_solver="$2"
                shift
                ;;
            *)
                echo "Invalid argument $1"
                return 0x80
                ;;
        esac
        shift
    done

    if test -z "${mbd_status_file}"; then
        echo "Invalid argument --status"
        return 0x80
    fi

    if test -z "${mbd_filename}"; then
        echo "Invalid argument --input"
        return 0x80
    fi

    if test "${idx_test}" -le 0; then
        echo "Invalid argument --index"
        return 0x80
    fi

    declare -i exit_status=-1

    status="unexpected"

    rm -f "${mbd_status_file}"

    expected_test_status=`awk -F '=' 'BEGIN{ status = -1; } /^[[:space:]]*##[[:space:]]*@MBDYN_SIMPLE_TESTSUITE_EXIT_STATUS@[[:space:]]*=[[:space:]]*[0-9]*[[:space:]]*$/ { status = ($2 != 0); } END{ printf("%d\n", status); }' "${mbd_filename}"`

    printf "%s: expected_test_status=%d\n" "${mbd_filename}" ${expected_test_status}

    if ! test -f "${mbd_filename}"; then
        echo "File \"${mbd_filename}\" not found"
        status=$(printf 'file[%]' "${mbd_filename}")
    else
        mbd_basename=`basename -s ".mbdyn" "${mbd_filename}"`
        mbd_basename=`basename -s ".mbd" "${mbd_basename}"`

        mbd_time_file="${mbdyn_testsuite_prefix_output}/${mbd_basename}_mbdyn_output_time_$((idx_test)).log"
        mbd_output_file="${mbdyn_testsuite_prefix_output}/${mbd_basename}_mbdyn_output_$((idx_test))"
        junit_xml_report_file="${mbdyn_testsuite_prefix_output}/junit_xml_report_${mbd_basename}_$((idx_test)).xml"

        case "${mbdyn_enable_gtest}" in
            yes)
                export GTEST_MBDYN_ARGS="-G --gtest_output=xml:${junit_xml_report_file}"
                ;;
            *)
                export GTEST_MBDYN_ARGS=""
                ;;
        esac

        ## Needed for all GNU-Octave scripts which are using mboct-mbdyn-pkg (e.g. "triangular_contact_run.m")
        export MBOCT_MBDYN_PKG_MBDYN_SOLVER_COMMAND="${MBDYN_EXEC} ${MBDYN_ARGS_ADD} ${GTEST_MBDYN_ARGS}"

        case "${OCTAVE_EXEC}" in
            gtest-*)
                ## Note: It should be safe to use the same name, since gtest will add an index if the file already exists
                GTEST_OCTAVE_ARGS="--gtest_output=xml:${junit_xml_report_file}"
                ;;
            *)
                GTEST_OCTAVE_ARGS=""
                ;;
        esac

        ## Make sure that we can use ${GTEST_OCTAVE_ARGS} in *_run.sh and *_gen.sh,
        ## since we do not know if ${OCTAVE_EXEC} will be called from those scripts.
        export GTEST_OCTAVE_ARGS

        mbd_log_file="${mbd_output_file}.stdout"

        mbd_script_name=`basename ${mbd_filename}`
        mbd_script_name=`basename -s .mbd ${mbd_script_name}`
        mbd_script_name=`basename -s .mbdyn ${mbd_script_name}`
        mbd_dir_name=`dirname "${mbd_filename}"`
        mbd_script_name_run_sh="${mbd_dir_name}/${mbd_script_name}_run.sh"
        mbd_script_name_gen_sh="${mbd_dir_name}/${mbd_script_name}_gen.sh"
        mbd_script_name_run_m="${mbd_dir_name}/${mbd_script_name}_run.m"
        mbd_script_name_gen_m="${mbd_dir_name}/${mbd_script_name}_gen.m"
        mbd_command=""

        if test "${mbdyn_patch_input}" != "no"; then
            ## FIXME: actually ${mbd_filename_patched} should be created inside the output directory.
            ## FIXME: However, MBDyn is not able to located additional input files, if ${mbd_filename_patched}
            ## FIXME: would be created inside a different directory than the original input file.
            mbd_filename_patched=$(mktemp -p "${mbd_dir_name}" "${mbd_basename}_XXXXXXXXXX_patched_$((idx_test)).mbd")
            mbd_filename_patched_copy="${mbdyn_testsuite_prefix_output}/${mbd_basename}_mbdyn_input_file_patched_$((idx_test)).mbd"

            if ! sed ${mbdyn_patch_input_sed_args} "${mbdyn_patch_input_sed_expression}" "${mbd_filename}" | tee "${mbd_filename_patched}" > "${mbd_filename_patched_copy}"; then
                rm -f "${mbd_filename_patched}"
                rm -f "${mbd_filename_patched_copy}"
                echo "Failed to patch input file \"${mbd_filename}\""
                return 1
            fi
        else
            mbd_filename_patched="${mbd_filename}"
        fi

        mbd_allow_patch="yes"

        for mbd_script_name in "${mbd_script_name_run_m}" "${mbd_script_name_gen_m}" "${mbd_script_name_run_sh}" "${mbd_script_name_gen_sh}"; do
            if ! test -z "${mbd_command}"; then
                break
            fi
            if test -f "${mbd_script_name}"; then
                echo "A custom test script ${mbd_script_name} was found for input file ${mbd_filename}; It will be used to run the model"
                case "${mbd_script_name}" in
                    *_gen.m|*_gen.sh)
                        ## It seems that all the Octave scripts from mbdyn-tests-public need to be executed only once, and their output may be reused for all patched tests.
                        if test "${mbd_exec_gen_script}" != "no"; then
                            mbd_command="${mbd_script_name} -f ${mbd_filename} -o ${mbd_output_file}"

                            case "${mbd_script_name}" in
                                *.m)
                                    mbd_command="${OCTAVE_EXEC} ${GTEST_OCTAVE_ARGS} -qf ${mbd_command}"
                                    ;;
                            esac

                            if test "${mbd_exec_solver}" != "yes"; then
                                ## Generate the input
                                mbd_exec_solver="yes"
                            else
                                ## Generate the input and execute MBDyn
                                mbd_command="${mbd_command}; ${MBDYN_EXEC} ${MBDYN_ARGS_ADD} -f ${mbd_filename} -o ${mbd_output_file} ${GTEST_MBDYN_ARGS}"
                            fi
                        fi
                        ;;
                    *_run.m)
                        if test "${mbd_exec_run_script}" != "no"; then
                            mbd_command="${OCTAVE_EXEC} ${GTEST_OCTAVE_ARGS} -q -f ${mbd_script_name} -f ${mbd_filename} -o ${mbd_output_file}"
                        else
                            mbd_allow_patch="no"
                        fi
                        ;;
                    *_run.sh)
                        if test "${mbd_exec_run_script}" != "no"; then
                            chmod +x "${mbd_script_name}"
                            mbd_command="${mbd_script_name} -f ${mbd_filename} -o ${mbd_output_file}"
                        else
                            mbd_allow_patch="no"
                        fi
                        ;;
                esac
            fi
        done

        mbd_exclude_test=`awk -f mbdyn_testsuite_exclude_test.awk "${mbd_filename}"`

        case "${mbd_exclude_test}" in
             excluded*)
                mbd_exec_solver="no"
                ;;
        esac

        if test ${skip_expected_failures} = "yes" && test ${expected_test_status} != "0"; then
            echo "Skipping test \"${mbd_filename}\" because it's expected to fail"
            mbd_exec_solver="no"
        elif test "${mbdyn_patch_input}" != "no" && test "${mbd_allow_patch}" != "yes"; then
            echo "Cannot execute test \"${mbd_filename}\""
            mbd_exec_solver="no"
        fi

        if test -z "${mbd_command}"; then
            echo "No custom test script was found for input file ${mbd_filename}; The default command will be used to run the model"
            mbd_command="${MBDYN_EXEC} ${MBDYN_ARGS_ADD} -f ${mbd_filename_patched} -o ${mbd_output_file} ${GTEST_MBDYN_ARGS}"
        fi

        case "${mbdyn_print_res}" in
            all|*time*)
                mbd_command="${TESTSUITE_TIME_CMD} --output ${mbd_time_file} ${mbd_command}"
                ;;
        esac

        if test -z "${mbdyn_testsuite_timeout}"; then
            mbdyn_testsuite_timeout="unlimited"
        fi

        case "${mbdyn_testsuite_timeout}" in
            unlimited)
                echo "No timeout is applied"
                ;;
            *)
                echo "Timeout after ${mbdyn_testsuite_timeout}"
                mbd_command="timeout --signal=SIGTERM ${mbdyn_testsuite_timeout} ${mbd_command}"
                ;;
        esac

        printf "%s:%s\n" "${mbd_basename}" "${mbd_command}"

        if test "${mbd_exec_solver}" != "no"; then
            curr_dir=`pwd`

            if ! cd "${mbd_dir_name}"; then
                echo "Invalid directory"
                return 0x80
            fi

            rm -f "${mbd_time_file}"
            rm -f "${mbd_log_file}"
            rm -f "${junit_xml_report_file}"

            ## Octave allows us to set TMPDIR in order to store all the temporary files in a single folder.
            ## This will make it easier to delete those files, just in case that we are using *_run.m to run the test case.
            export TMPDIR="${mbdyn_testsuite_prefix_output}"

            if test "${mbdyn_verbose_output}" = "yes"; then
                mbd_command="${mbd_command} |& tee ${mbd_log_file}"
            else
                mbd_command="${mbd_command} >& ${mbd_log_file}"
            fi

            eval ${mbd_command}

            ((rc=$?))

            if ! cd "${curr_dir}"; then
                echo "Invalid directory"
                return 0x80
            fi
        else
            ## According to "man(3) exit", status & 0xFF is returned to the parent
            ((rc=-1))
        fi

        if test "${mbdyn_patch_input}" != "no"; then
            ## Must be deleted in any case because it is located inside the input directory!
            ## In case of failure, we will just keep "${mbd_filename_patched_copy}"
            rm -f "${mbd_filename_patched}"
        fi

        case $((rc)) in
            -1)
                if test "${mbd_exec_solver}" = "no"; then
                    status="skipped"
                else
                    status="unexpected"
                fi
                ;;
            0)
                status="passed"

                num_steps=`awk 'BEGIN{num_steps=0}/^End of simulation at time [0-9.-]+ after [0-9]+ steps;$/{num_steps=$8} END{print num_steps}' "${mbd_log_file}"`

                ## Let's check also the output files and do not rely just on a zero exit status!
                if test -f "${junit_xml_report_file}" && awk -f parse_test_suite_status.awk "${junit_xml_report_file}" >& /dev/null; then
                    status=$(printf 'passed{Steps=%d}' "${num_steps}")
                fi
                ;;
            1)
                mbd_error_info=`awk -v suppressed_errors="${mbdyn_suppressed_errors}" -f parse_mbdyn_error_message.awk "${mbd_log_file}"`

                if test $? -eq 0; then
                    status="suppressed:${mbd_error_info}"
                else
                    case "${mbd_error_info}" in
                        module*|loadable*)
                            status="module"
                            ;;
                        *)
                            status="failed"
                            ;;
                    esac
                fi
                ;;
            124)
                status="timeout"
                ;;
            130)
                status="interrupted"
                ;;
            143)
                status="terminated"
                ;;
            137)
                status="killed"
                ;;
            *)
                status="unexpected"
                ;;
        esac

        printf "Test \"%s\" returned exit status %d (%s)\n" "${mbd_basename}" "${rc}" "${status}"

        if test -f "${mbd_time_file}"; then
            cat "${mbd_time_file}"
        fi

        keep_output_flag="no"

        case "${mbdyn_keep_output}" in
            all)
                keep_output_flag="yes"
                ;;
            failed)
                case "${status}" in
                    failed|unexpected)
                        keep_output_flag="yes"
                        ;;
                esac
                ;;
            unexpected)
                case "${status}" in
                    unexpected)
                        keep_output_flag="yes"
                        ;;
                esac
                ;;
        esac

        if test "${keep_output_flag}" = "no"; then
            mbd_real_output_file=""

            if test -f "${mbd_log_file}"; then
                ## If we were executing a script, MBDyn's -o option might be ignored
                mbd_real_output_file=$(awk -F '"' '/^output in file\>/{print $2}' "${mbd_log_file}")
            fi

            ## FIXME: If we use "abort after: derivatives;", then MBDyn does not print the output file name, although output might be generated.
            if ! test -z "${mbd_real_output_file}"; then
                mbd_output_file="${mbd_real_output_file}"
            fi

            echo "Clean up output in \"${mbd_output_file}\""

            if test -f "${mbd_output_file}.log"; then
                ## Need to remove also _[0-9]*.m files
                mbd_output_file_pattern=`printf '%s*' "${mbd_output_file}"`
                find "${mbdyn_testsuite_prefix_output}" '(' -type f -and -wholename "${mbd_output_file_pattern}" ')' -delete
            fi

            rm -f "${mbd_log_file}"
            rm -f "${mbd_time_file}"

            ## FIXME: Actually we should keep all those files.
            ## FIXME: But due to the huge number of tests GitLab-CI is not able to display all of them.
            ## FIXME: So, by default, only failed test reports will be kept.
            junit_xml_keep_output="yes"

            case "${JUNIT_XML_KEEP_ALL_OUTPUT}" in
                always)
                    ;;
                not-passed)
                    case "${status}" in
                        passed*|timeout*)
                            junit_xml_keep_output="no"
                        ;;
                    esac
                    ;;
                failed)
                    case "${status}" in
                        failed*|unexpected*)
                            ;;
                        *)
                            junit_xml_keep_output="no"
                            ;;
                    esac
                    ;;
                none)
                    junit_xml_keep_output="no"
                    ;;
            esac

            if test "${junit_xml_keep_output}" != "yes"; then
                if test -f "${junit_xml_report_file}"; then
                    echo "File ${junit_xml_report_file} will be removed (status=${status}, JUNIT_XML_KEEP_ALL_OUTPUT=${JUNIT_XML_KEEP_ALL_OUTPUT})"
                    if awk -f parse_test_suite_status.awk "${junit_xml_report_file}" > /dev/null; then
                        rm -f "${junit_xml_report_file}"
                    fi
                fi
            fi

            if test "${mbdyn_patch_input}" != "no"; then
                if ! test -f "${mbd_filename_patched_copy}"; then
                    echo "File not found: \"${mbd_filename_patched_copy}\""
                fi
                rm -f "${mbd_filename_patched_copy}"
            else
                echo "File \"${mbd_filename}\" was not patched"
            fi
        else
            echo "Keep output in \"${mbd_output_file}\""
        fi
    fi

    case "${status}" in
        skipped)
            ## Skipped tests are not considered as an error
            ((exit_status=0x0))
            ;;
        passed*)
            ((exit_status=0x0))
            ;;
        file*)
            ((exit_status=0x1))
            ;;
        timeout)
            ((exit_status=0x2))
            ;;
        suppressed*)
            ((exit_status=0x4))
            ;;
        module|loadable)
            ((exit_status=0x8))
            ;;
        failed)
            ((exit_status=0x10))
            ;;
        *)
            ((exit_status=0x40))
            ;;
    esac

    if test $((exit_status)) -eq 0; then
        ((test_status=0))
    else
        ((test_status=1))
    fi

    if ! test "${update_reference_test_status}" = "no"; then
        if test "${expected_test_status}" -eq -1; then
            printf '\n##############################################################################################################\n' >> "${mbd_filename}"
            printf '## Variables to be updated by simple_testsuite.sh --update-reference-test-status\n' >> "${mbd_filename}"
            printf '## Warning, do not edit!!!\n' >> "${mbd_filename}"
            printf '## @MBDYN_SIMPLE_TESTSUITE_EXIT_STATUS@ = %d\n' $((exit_status)) >> "${mbd_filename}"
            printf '##############################################################################################################\n' >> "${mbd_filename}"
        else
            do_update_file="no"

            case "${update_reference_test_status}" in
                failed)
                    if test $((test_status)) -ne 0 && test $(( exit_status != 0x4 )) -ne 0; then
                        do_update_file="yes"
                    fi
                    ;;
                passed)
                    if test $((test_status)) -eq 0 && test $(( exit_status != 0x4 )) -ne 0; then
                        do_update_file="yes"
                    fi
                    ;;
                all|yes)
                    if test $(( exit_status != 0x4 )) -ne 0; then
                        do_update_file="yes"
                    fi
                    ;;
            esac

            if test "${do_update_file}" = "yes"; then
                sed -i "s/^[[:space:]]*\#\#[[:space:]]*@MBDYN_SIMPLE_TESTSUITE_EXIT_STATUS@[[:space:]]*=[[:space:]]*[0-9]*[[:space:]]*$/\#\# @MBDYN_SIMPLE_TESTSUITE_EXIT_STATUS@ = $((test_status))/g;" "${mbd_filename}"
            fi
        fi

        expected_test_status=$((test_status))
    fi

    if test "${use_reference_test_status}" = "yes"; then
        if test $((test_status)) -eq $((expected_test_status)); then
            if test $((test_status)) -ne 0; then
                status="known-failure-${status}"
                ((exit_status=0x0))
            fi
        else
            if test $((test_status)) -ne 0; then
                status="regression-${status}"
            else
                status="fixed-failure"
            fi
        fi
    fi

    printf "%s(%d:%d:%d)\n" "${status}" ${rc} $((exit_status)) $((expected_test_status)) > "${mbd_status_file}"

    return $((exit_status))
}

mbdyn_simple_testsuite_pid=$$

mbd_status_file_format=`printf '%s/mbdyn_simple_testsuite_run_test_%08X_%%s.status' "${mbdyn_testsuite_prefix_output}" "${mbdyn_simple_testsuite_pid}"`

printf '%d valid input files were found\n' `echo ${MBD_INPUT_FILES_FOUND} | wc -w`

((idx_test=0))
for mbd_filename in ${MBD_INPUT_FILES_FOUND}; do
    ((++idx_test))
    printf "%4d: \"%s\"\n" $((idx_test)) "${mbd_filename}"
    mbd_status_file=`printf "${mbd_status_file_format}" $((idx_test))`
    ## Make sure, that all tests were really executed.
    rm -f "${mbd_status_file}"
done

## Ensure that we can use those variables within all *_run.sh, *_gen.sh, *_run.m and *_gen.m scripts
export MBDYN_EXEC
export OCTAVE_EXEC
export PYTHON_EXEC

if test $((idx_test)) -le 1 || test "${MBD_NUM_TASKS}" -le 1; then
    ## Sequential execution
    if test "${mbdyn_exec_gen}" != "no"; then
        ## Sequential execution of *_gen.m scripts
        ## Note: Make sure that all the *_gen.m scripts were executed before any model is executed!
        ## Note: That's because some models have only one *_gen.m script but multiple models are using the output from the same *_gen.m script.
        ((idx_test=0))
        for mbd_filename in ${MBD_INPUT_FILES_FOUND}; do
            ((++idx_test))
            mbd_status_file=`printf "${mbd_status_file_format}" $((idx_test))`
            simple_testsuite_run_test --status "${mbd_status_file}" --input "${mbd_filename}" --index "${idx_test}" --exec-solver no
        done
    fi

    if test "${mbdyn_exec_solver}" != "no"; then
        ((idx_test=0))
        for mbd_filename in ${MBD_INPUT_FILES_FOUND}; do
            ((++idx_test))
            mbd_status_file=`printf "${mbd_status_file_format}" $((idx_test))`
            simple_testsuite_run_test --status "${mbd_status_file}" --input "${mbd_filename}" --index "${idx_test}" --exec-gen no
        done
    fi
else
    ## Parallel execution
    export mbdyn_testsuite_prefix_output
    export mbdyn_sed_prefix
    export mbdyn_patch_input
    export mbdyn_testsuite_timeout
    export mbdyn_verbose_output
    export mbdyn_keep_output
    export mbdyn_print_res
    export mbdyn_suppressed_errors
    export mbdyn_enable_gtest
    export update_reference_test_status
    export use_reference_test_status
    export skip_expected_failures
    export MBDYN_EXEC
    export MBDYN_ARGS_ADD
    export MBD_NUM_THREADS
    export JUNIT_XML_KEEP_ALL_OUTPUT
    export -f simple_testsuite_run_test
    export mbdyn_patch_input_sed_expression
    export mbdyn_patch_input_sed_args

    if test "${mbdyn_exec_gen}" != "no"; then
        ## Sequential execution of *_gen.m scripts
        ((idx_test=0))
        for mbd_filename in ${MBD_INPUT_FILES_FOUND}; do
            ((++idx_test))
            mbd_status_file=`printf "${mbd_status_file_format}" $((idx_test))`
            simple_testsuite_run_test --status "${mbd_status_file}" --input "${mbd_filename}" --index "${idx_test}" --exec-solver no
        done
    fi

    if test "${mbdyn_exec_solver}" != "no"; then
        ## Parallel execution of the solver
        mbd_status_file=`printf "${mbd_status_file_format}" '{#}'`
        mbd_parallel_args="-j${MBD_NUM_TASKS} -n1 simple_testsuite_run_test --status ${mbd_status_file} --input '{}' --index '{#}' --exec-gen no"
        printf '%s\n' ${MBD_INPUT_FILES_FOUND} | parallel ${mbd_parallel_args}
    fi
fi

((idx_test=0))
for mbd_filename in ${MBD_INPUT_FILES_FOUND}; do
    ((++idx_test))

    printf "%4d: \"%s\"\n" $((idx_test)) "${mbd_filename}"

    mbd_status_file=`printf "${mbd_status_file_format}" $((idx_test))`

    status="unexpected"

    if test -f "${mbd_status_file}"; then
        status=`cat ${mbd_status_file}`
    fi

    rm -f "${mbd_status_file}"

    case "${status}" in
        skipped*)
            skipped_tests="${skipped_tests} ${mbd_filename}:${status}"
            ;;
        passed*)
            passed_tests="${passed_tests} ${mbd_filename}:${status}"
            ;;
        timeout*)
            timeout_tests="${timeout_tests} ${mbd_filename}:${status}"
            ;;
        module*)
            modules_not_found="${modules_not_found} ${mbd_filename}:${status}"
            ;;
        suppressed*)
            suppressed_failures="${suppressed_failures} ${mbd_filename}:${status}"
            ;;
        failed*)
            failed_tests="${failed_tests} ${mbd_filename}:${status}"
            ;;
        regression*)
            regressions="${regressions} ${mbd_filename}:${status}"
            ;;
        known-failure*)
            known_failures="${known_failures} ${mbd_filename}:${status}"
            ;;
        fixed-failure*)
            fixed_failures="${fixed_failures} ${mbd_filename}:${status}"
            ;;
        *)
            unexpected_faults="${unexpected_faults} ${mbd_filename}:${status}"
            ;;
    esac
done

declare -i exit_status=0x0

function print_files()
{
    declare -i idx=0
    format="$1"
    shift

    printf "${format}" "$(echo ${*} | wc -w)"

    for mbd_filename in $*; do
        printf "%4d:%s\n" "$((++idx))" "${mbd_filename}"
    done
}

function report_tests()
{
    if test -z "$1"; then
        echo "$2"
        return 1
    else
        print_files "$3" $1
    fi
}

printf "@BEGIN_SIMPLE_TESTSUITE_REPORT@\n"

if ! report_tests "${passed_tests}" "No tests passed" "PASSED:The following %d tests passed with zero exit status:\n"; then
    ((exit_status|=0x1))
fi

if report_tests "${timeout_tests}" "No tests were killed because of timeout" "TIMEOUT:The following %d tests were killed because of timeout:\n"; then
    ((exit_status|=0x2))
fi

if report_tests "${modules_not_found}" "All modules were found" "FAILED-MODULE:The following %d tests failed because a loadable module was not found:\n"; then
    ((exit_status|=0x4))
fi

if report_tests "${suppressed_failures}" "There were no suppressed failures" "FAILED-SUPPRESSED:The following %d failures were suppressed:\n"; then
    ((exit_status|=0x8))
fi

if report_tests "${failed_tests}" "No tests failed with status 1" "FAILED:The following %d tests failed with status 1:\n"; then
   ((exit_status|=0x10))
fi

if report_tests "${regressions}" "No regressions" "REGRESSIONS:The following %d tests observed a regression:\n"; then
   ((exit_status|=0x20))
fi

if report_tests "${unexpected_faults}" "No tests returned with unexpected exit status" "FAILED-UNEXPECTED:The following %d tests failed with unexpected exit status:\n"; then
    ((exit_status|=0x40))
fi

if report_tests "${fixed_failures}" "No failures were fixed" "FIXED-FAILURES:The following %d tests were fixed:\n"; then
   ((exit_status|=0x80))
fi

if report_tests "${known_failures}" "There were no known failures" "KNOWN-FAILURES:There were %d known failures:\n"; then
    ((exit_status|=0x100))
fi

if report_tests "${skipped_tests}" "No tests were skipped" "SKIPPED:The following %d tests were skipped:\n"; then
    ((exit_status|=0x200))
fi

if test $((exit_status&~mbd_exit_status_mask)) == 0 && test $((exit_status)) != 0; then
    printf "Minor failures (0x%X) were detected during this test, but they will be ignored!\n" $((exit_status&mbd_exit_status_mask))
fi

## Mask all the error codes which should be ignored for the current test
((exit_status&=~mbd_exit_status_mask))

printf "${program_name} exit status 0x%X\n" $((exit_status))

printf "@END_SIMPLE_TESTSUITE_REPORT@\n"

exit $((exit_status))
