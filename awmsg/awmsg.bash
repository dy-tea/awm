if [[ $BASH_VERSINFO -lt 4 ]]; then
    echo "This completion script requires bash 4.0 or newer (current is $BASH_VERSION)"
    exit 1
fi

__complgen_match () {
    [[ $# -lt 2 ]] && return 1
    local ignore_case=$1
    local prefix=$2
    [[ -z $prefix ]] && cat
    if [[ $ignore_case = on ]]; then
        prefix=${prefix,,}
        prefix=$(printf '%q' "$prefix")
        while read line; do
            [[ ${line,,} = ${prefix}* ]] && echo $line
        done
    else
        prefix=$(printf '%q' "$prefix")
        while read line; do
            [[ $line = ${prefix}* ]] && echo $line
        done
    fi
}

_awmsg () {
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-candidates system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    declare -a literals=(help exit spawn -c --continuous output list modes workspace list set toplevel list keyboard list device list current bind list run display)
    declare -a regexes=()
    declare -A literal_transitions=()
    declare -A nontail_transitions=()
    literal_transitions[0]="([0]=1 [1]=1 [2]=2 [3]=3 [4]=3 [5]=4 [8]=5 [11]=6 [13]=7 [15]=8 [18]=9)"
    literal_transitions[3]="([3]=3 [4]=3 [5]=4 [8]=5 [11]=6 [13]=7 [15]=8 [18]=9)"
    literal_transitions[4]="([6]=1 [7]=1)"
    literal_transitions[5]="([9]=1 [10]=11)"
    literal_transitions[6]="([12]=1)"
    literal_transitions[7]="([14]=1)"
    literal_transitions[8]="([16]=1 [17]=1)"
    literal_transitions[9]="([19]=1 [20]=10 [21]=10)"
    declare -A match_anything_transitions=([11]=1 [10]=1 [2]=1)
    declare -A subword_transitions

    local state=0
    local word_index=1
    while [[ $word_index -lt $cword ]]; do
        local word=${words[$word_index]}

        if [[ -v "literal_transitions[$state]" ]]; then
            declare -A state_transitions
            eval "state_transitions=${literal_transitions[$state]}"

            local word_matched=0
            for literal_id in $(seq 0 $((${#literals[@]} - 1))); do
                if [[ ${literals[$literal_id]} = "$word" ]]; then
                    if [[ -v "state_transitions[$literal_id]" ]]; then
                        state=${state_transitions[$literal_id]}
                        word_index=$((word_index + 1))
                        word_matched=1
                        break
                    fi
                fi
            done
            if [[ $word_matched -ne 0 ]]; then
                continue
            fi
        fi

        if [[ -v "match_anything_transitions[$state]" ]]; then
            state=${match_anything_transitions[$state]}
            word_index=$((word_index + 1))
            continue
        fi

        return 1
    done

    declare -A literal_transitions_level_0=([6]="12" [7]="14" [0]="0 1 2 3 4 5 8 11 13 15 18" [8]="16 17" [3]="3 4 5 8 11 13 15 18" [9]="19 20 21" [5]="9 10" [4]="6 7")
    declare -A subword_transitions_level_0=()
    declare -A commands_level_0=()

    local -a candidates=()
    local -a matches=()
    local ignore_case=$(bind -v | grep completion-ignore-case | cut -d' ' -f3)
    local max_fallback_level=0
    local prefix="${words[$cword]}"
    for (( fallback_level=0; fallback_level <= max_fallback_level; fallback_level++ )) {
        eval "declare literal_transitions_name=literal_transitions_level_${fallback_level}"
        eval "declare -a transitions=(\${$literal_transitions_name[$state]})"
        for literal_id in "${transitions[@]}"; do
            local literal="${literals[$literal_id]}"
            candidates+=("$literal ")
        done
        if [[ ${#candidates[@]} -gt 0 ]]; then
            readarray -t matches < <(printf "%s\n" "${candidates[@]}" | __complgen_match "$ignore_case" "$prefix")
        fi

        eval "declare subword_transitions_name=subword_transitions_level_${fallback_level}"
        eval "declare -a transitions=(\${$subword_transitions_name[$state]})"
        for subword_id in "${transitions[@]}"; do
            readarray -t -O "${#matches[@]}" matches < <(_awmsg_subword_$subword_id complete "$prefix")
        done

        eval "declare commands_name=commands_level_${fallback_level}"
        eval "declare -a transitions=(\${$commands_name[$state]})"
        for command_id in "${transitions[@]}"; do
            readarray -t candidates < <(_awmsg_cmd_$command_id "$prefix" | cut -f1)
            if [[ ${#candidates[@]} -gt 0 ]]; then
                readarray -t -O "${#matches[@]}" matches < <(printf "%s\n" "${candidates[@]}" | __complgen_match "$ignore_case" "$prefix")
            fi
        done

        if [[ ${#matches[@]} -gt 0 ]]; then
            local shortest_suffix="$prefix"
            for ((i=0; i < ${#COMP_WORDBREAKS}; i++)); do
                local char="${COMP_WORDBREAKS:$i:1}"
                local candidate=${prefix##*$char}
                if [[ ${#candidate} -lt ${#shortest_suffix} ]]; then
                    shortest_suffix=$candidate
                fi
            done
            local superfluous_prefix=""
            if [[ "$shortest_suffix" != "$prefix" ]]; then
                local superfluous_prefix=${prefix%$shortest_suffix}
            fi
            COMPREPLY=("${matches[@]#$superfluous_prefix}")
            break
        fi
    }

    return 0
}

complete -o nospace -F _awmsg awmsg
