#!/usr/bin/env bash

set -eu

amru_prefix=$dfsu_prefix

#
# amr_prepare_expdir: prepare variables for a single experiment
# and create the expdir. must be done before amr_prepare_deck
#
# args: arg_amr_bin, arg_lb_policy
# uses: cores, nodes
# sets: exp_tag, exp_jobdir, exp_logfile
# creates: exp_jobdir
#
amr_prepare_expdir() {
  local bin_name=$(basename ${1})
  local policy=${2}
  exp_tag="${bin_name}_LB${policy}_C${cores}_N${nodes}"

  message "-INFO- Will use exp dir: $exp_tag in jobdir"
  exp_jobdir="$jobdir/$exp_tag"
  exp_logfile="$exp_jobdir/$exp_tag.log"

  mkdir -p $exp_jobdir
}

#
# amr_prepare_deck: prepare the AMR deck
#
# args: arg_amr_deck, arg_lb_policy, arg_nlim
# uses:
# creates:

amr_prepare_deck() {
  local deck_name=${1}
  local deck_lb_policy=${2}
  local deck_nlim=${3}

  local deck_in_fpath="$amru_prefix/decks/${deck_name}.in"
  local deck_out_fpath="$exp_jobdir/${deck_name}"

  [ -f "$deck_in_fpath" ] || die "!! ERROR !! Deck file not found: $deck_in_fpath"

  message "-INFO- Preparing deck from template: $deck_in_fpath"
  message "-INFO- Generated deck will be: $deck_out_fpath"

  cat $deck_in_fpath |
    sed -s "s/{LB_POLICY}/$deck_lb_policy/g" |
    sed -s "s/{NLIM}/$deck_nlim/g" >$deck_out_fpath

  message "-INFO- Deck prepared"
}

#
# amr_do_run: run an AMR experiment
#
# args: arg_pre, arg_amr_bin, arg_amr_deck, arg_nodes, arg_cpubind
# uses: amru_prefix, cores, ppn, logfile, exp_logfile, arg_amr_monp2p_reduce,
#       arg_amr_monp2p_put, arg_amr_mon_topk, arg_amr_glog_minloglevel,
#       arg_amr_glog_v
#
# creates:
# side effects: changes current directory to $exp_jobdir
#
# Arguments:

amr_do_run() {
  local prelib="${1}"
  local amr_bin="${2}"
  local amr_deck="${3}"
  local amr_nodes="${4}"
  local amr_cpubind="${5}"

  local amr_bin_fullpath="$amru_prefix/bin/$amr_bin"
  local amr_deck_fullpath="$exp_jobdir/$amr_deck"
  local amr_exec="$amr_bin_fullpath -i $amr_deck_fullpath"

  [ -f "$amr_bin_fullpath" ] || die "AMR binary not found: $amr_bin_fullpath"
  [ -f "$amr_deck_fullpath" ] || die "AMR deck not found: $amr_deck_fullpath"

  message "-INFO- Running AMR experiment: $amr_exec"

  prelib_env=()
  if [ x"$prelib" != x ]; then
    # If prelib is a relative path, check for its existence in lib/
    if [ "${prelib:0:1}" != "/" ]; then
      prelib="$amru_prefix/lib/$prelib"
      [ -f "$prelib" ] || die "Preload lib not found: $prelib"
    fi

    prelib_env=(
      "LD_PRELOAD" "$prelib"
      "KOKKOS_TOOLS_LIBS" "$prelib"
    )
  fi

  # create an array called env_vars, and insert prelib_env into that array
  env_vars=(
    "${prelib_env[@]}"
    "AMRMON_PRINT_TOPK" "${arg_amr_mon_topk}"
    "AMRMON_P2P_ENABLE_REDUCE" "${arg_amr_monp2p_reduce}"
    "AMRMON_P2P_ENABLE_PUT" "${arg_amr_monp2p_put}"
    "GLOG_minloglevel" "${arg_amr_glog_minloglevel}"
    "GLOG_v" "${arg_amr_glog_v}"
  )

  do_mpirun $cores $ppn "$amr_cpubind" env_vars[@] \
    "$amr_nodes" "$amr_exec" "${EXTRA_MPIOPTS-}"
}
