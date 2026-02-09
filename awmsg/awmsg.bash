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
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-completion system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    local -a literals=(-h --help -v --version exit spawn -c --continuous -1 --1-line -s --socket output list toplevels modes create destroy workspace list set toplevel list focused keyboard list device list current bind list run none maximize fullscreen previous next move resize pin toggle_floating up down left right close swap_up swap_down swap_left swap_right half_up half_down half_left half_right tile tile_sans auto_tile open window_to display rule list)
    local -A literal_transitions=()
    literal_transitions[0]="([0]=1 [1]=1 [2]=1 [3]=1 [4]=1 [5]=2 [6]=3 [7]=3 [8]=3 [9]=3 [10]=4 [11]=4 [12]=5 [18]=6 [21]=7 [24]=8 [26]=9 [29]=10 [60]=11)"
    literal_transitions[3]="([6]=3 [7]=3 [8]=3 [9]=3 [10]=4 [11]=4 [12]=5 [18]=6 [21]=7 [24]=8 [26]=9 [29]=10 [60]=11)"
    literal_transitions[5]="([13]=1 [14]=1 [15]=1 [16]=2 [17]=2)"
    literal_transitions[6]="([19]=1 [20]=2)"
    literal_transitions[7]="([22]=1 [23]=1)"
    literal_transitions[8]="([25]=1)"
    literal_transitions[9]="([27]=1 [28]=1)"
    literal_transitions[10]="([30]=1 [31]=12 [59]=12)"
    literal_transitions[11]="([61]=1)"
    literal_transitions[12]="([4]=1 [32]=1 [33]=1 [34]=1 [35]=1 [36]=1 [37]=1 [38]=1 [39]=1 [40]=1 [41]=1 [42]=1 [43]=1 [44]=1 [45]=1 [46]=1 [47]=1 [48]=1 [49]=1 [50]=1 [51]=1 [52]=1 [53]=1 [54]=1 [55]=1 [56]=1 [57]=2 [58]=2)"
    local -A star_transitions=([2]=1)

    local state=0
    local word_index=1
    while [[ $word_index -lt $cword ]]; do
        local word=${words[$word_index]}

        if [[ -v "literal_transitions[$state]" ]]; then
            local -A state_transitions
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

        if [[ -v "star_transitions[$state]" ]]; then
            state=${star_transitions[$state]}
            word_index=$((word_index + 1))
            continue
        fi

        return 1
    done

    local -A literal_transitions_level_0=([9]="27 28" [0]="0 1 2 3 4 5 6 7 8 9 10 11 12 18 21 24 26 29 60" [3]="6 7 8 9 10 11 12 18 21 24 26 29 60" [8]="25" [10]="30 31 59" [11]="61" [5]="13 14 15 16 17" [12]="4 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58" [7]="22 23" [6]="19 20")
    local -A commands_level_0=([4]="0")

    local -a candidates=()
    local -a matches=()
    local ignore_case=$(bind -v | grep completion-ignore-case | cut -d' ' -f3)
    local max_fallback_level=0
    local prefix="${words[$cword]}"
    for (( fallback_level=0; fallback_level <= max_fallback_level; fallback_level++ )) {
        eval "local literal_transitions_name=literal_transitions_level_${fallback_level}"
        eval "local -a transitions=(\${$literal_transitions_name[$state]})"
        for literal_id in "${transitions[@]}"; do
            local literal="${literals[$literal_id]}"
            candidates+=("$literal ")
        done
        if [[ ${#candidates[@]} -gt 0 ]]; then
            readarray -t matches < <(printf "%s\n" "${candidates[@]}" | __complgen_match "$ignore_case" "$prefix")
        fi
        eval "local commands_name=commands_level_${fallback_level}"
        eval "local -a transitions=(\${$commands_name[$state]})"
        for command_id in "${transitions[@]}"; do
            readarray -t candidates < <(_awmsg_cmd_$command_id "$prefix" "" | cut -f1)
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
