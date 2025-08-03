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

_awmsg_cmd_0 () {
    compgen -A file "$1"
}

_awmsg () {
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-candidates system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    declare -a literals=(-h --help -v --version exit spawn -c --continuous -1 --1-line -s --socket output list toplevels modes workspace list set toplevel list keyboard list device list current bind list run none maximize fullscreen previous next move up down left right close swap_up swap_down swap_left swap_right half_up half_down half_left half_right tile tile_sans open window_to display rule list)
    declare -a regexes=()
    declare -A literal_transitions=()
    declare -A nontail_transitions=()
    literal_transitions[0]="([0]=1 [1]=1 [2]=1 [3]=1 [4]=1 [5]=2 [6]=3 [7]=3 [8]=3 [9]=3 [10]=4 [11]=4 [12]=5 [16]=6 [19]=7 [21]=8 [23]=9 [26]=10 [53]=11)"
    literal_transitions[3]="([6]=3 [7]=3 [8]=3 [9]=3 [10]=4 [11]=4 [12]=5 [16]=6 [19]=7 [21]=8 [23]=9 [26]=10 [53]=11)"
    literal_transitions[5]="([13]=1 [14]=1 [15]=1)"
    literal_transitions[6]="([17]=1 [18]=14)"
    literal_transitions[7]="([20]=1)"
    literal_transitions[8]="([22]=1)"
    literal_transitions[9]="([24]=1 [25]=1)"
    literal_transitions[10]="([27]=1 [28]=12 [52]=12)"
    literal_transitions[11]="([54]=1)"
    literal_transitions[12]="([4]=1 [29]=1 [30]=1 [31]=1 [32]=1 [33]=1 [34]=1 [35]=1 [36]=1 [37]=1 [38]=1 [39]=1 [40]=1 [41]=1 [42]=1 [43]=1 [44]=1 [45]=1 [46]=1 [47]=1 [48]=1 [49]=1 [50]=13 [51]=13)"
    declare -A match_anything_transitions=([2]=1 [13]=1 [4]=3 [14]=1)
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

    declare -A literal_transitions_level_0=([11]="54" [6]="17 18" [7]="20" [0]="0 1 2 3 4 5 6 7 8 9 10 11 12 16 19 21 23 26 53" [8]="22" [3]="6 7 8 9 10 11 12 16 19 21 23 26 53" [5]="13 14 15" [9]="24 25" [10]="27 28 52" [12]="4 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51")
    declare -A subword_transitions_level_0=()
    declare -A commands_level_0=([4]="0")

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
