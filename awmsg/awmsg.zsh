#compdef awmsg

_awmsg_cmd_0 () {
    _path_files
}

_awmsg () {
    local -a literals=("-h" "--help" "-v" "--version" "exit" "spawn" "-c" "--continuous" "-1" "--1-line" "-s" "--socket" "output" "list" "modes" "workspace" "list" "set" "toplevel" "list" "keyboard" "list" "device" "list" "current" "bind" "list" "run" "none" "maximize" "fullscreen" "previous" "next" "move" "up" "down" "left" "right" "close" "swap_up" "swap_down" "swap_left" "swap_right" "half_up" "half_down" "half_left" "half_right" "tile" "tile_sans" "open" "window_to" "display")

    local -A descriptions
    descriptions[1]="show help"
    descriptions[2]="show help"
    descriptions[3]="show version"
    descriptions[4]="show version"
    descriptions[5]="exit awm"
    descriptions[6]="spawn a command"
    descriptions[7]="keep writing updates until cancelled"
    descriptions[8]="keep writing updates until cancelled"
    descriptions[9]="write on a single line"
    descriptions[10]="write on a single line"
    descriptions[11]="use socket path"
    descriptions[12]="use socket path"
    descriptions[14]="list outputs"
    descriptions[15]="list output modes"
    descriptions[17]="list workspaces"
    descriptions[18]="set current workspace to num"
    descriptions[20]="list toplevels"
    descriptions[22]="list keyboards"
    descriptions[24]="list devices"
    descriptions[25]="show current device"
    descriptions[27]="list key bindings"
    descriptions[28]="run key binding for name"
    descriptions[29]="do nothing"
    descriptions[30]="maximize the active window"
    descriptions[31]="fullscreen the active window"
    descriptions[32]="focus the previous window"
    descriptions[33]="focus the next window"
    descriptions[34]="start an interactive move with the active window"
    descriptions[35]="focus the window in the up direction"
    descriptions[36]="focus the window in the down direction"
    descriptions[37]="focus the window in the left direction"
    descriptions[38]="focus the window in the right direction"
    descriptions[39]="close the active window"
    descriptions[40]="swap the active window with the window in the up direction"
    descriptions[41]="swap the active window with the window in the down direction"
    descriptions[42]="swap the active window with the window in the left direction"
    descriptions[43]="swap the active window with the window in the right direction"
    descriptions[44]="half the active window in the up direction"
    descriptions[45]="half the active window in the down direction"
    descriptions[46]="half the active window in the left direction"
    descriptions[47]="half the active window in the right direction"
    descriptions[48]="tile all windows in the active workspace"
    descriptions[49]="tile all windows in the active workspace excluding the active one"
    descriptions[50]="focus workspace N"
    descriptions[51]="move the active window to workspace N"
    descriptions[52]="display key binding for name"

    local -A literal_transitions
    literal_transitions[1]="([1]=2 [2]=2 [3]=2 [4]=2 [5]=2 [6]=3 [7]=4 [8]=4 [9]=4 [10]=4 [11]=5 [12]=5 [13]=6 [16]=7 [19]=8 [21]=9 [23]=10 [26]=11)"
    literal_transitions[4]="([7]=4 [8]=4 [9]=4 [10]=4 [11]=5 [12]=5 [13]=6 [16]=7 [19]=8 [21]=9 [23]=10 [26]=11)"
    literal_transitions[6]="([14]=2 [15]=2)"
    literal_transitions[7]="([17]=2 [18]=14)"
    literal_transitions[8]="([20]=2)"
    literal_transitions[9]="([22]=2)"
    literal_transitions[10]="([24]=2 [25]=2)"
    literal_transitions[11]="([27]=2 [28]=12 [52]=12)"
    literal_transitions[12]="([5]=2 [29]=2 [30]=2 [31]=2 [32]=2 [33]=2 [34]=2 [35]=2 [36]=2 [37]=2 [38]=2 [39]=2 [40]=2 [41]=2 [42]=2 [43]=2 [44]=2 [45]=2 [46]=2 [47]=2 [48]=2 [49]=2 [50]=13 [51]=13)"

    local -A match_anything_transitions
    match_anything_transitions=([3]=2 [5]=4 [13]=2 [14]=2)

    declare -A subword_transitions

    local state=1
    local word_index=2
    while [[ $word_index -lt $CURRENT ]]; do
        if [[ -v "literal_transitions[$state]" ]]; then
            local -A state_transitions
            eval "state_transitions=${literal_transitions[$state]}"

            local word=${words[$word_index]}
            local word_matched=0
            for ((literal_id = 1; literal_id <= $#literals; literal_id++)); do
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
    declare -A literal_transitions_level_0=([12]="5 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51" [7]="17 18" [8]="20" [1]="1 2 3 4 5 6 7 8 9 10 11 12 13 16 19 21 23 26" [4]="7 8 9 10 11 12 13 16 19 21 23 26" [9]="22" [6]="14 15" [10]="24 25" [11]="27 28 52")
    declare -A subword_transitions_level_0=()
    declare -A commands_level_0=()
    declare -A specialized_commands_level_0=([5]="0")

    local max_fallback_level=0
    for (( fallback_level=0; fallback_level <= max_fallback_level; fallback_level++ )) {
        completions_no_description_trailing_space=()
        completions_no_description_no_trailing_space=()
        completions_trailing_space=()
        suffixes_trailing_space=()
        descriptions_trailing_space=()
        completions_no_trailing_space=()
        suffixes_no_trailing_space=()
        descriptions_no_trailing_space=()
        matches=()

        declare literal_transitions_name=literal_transitions_level_${fallback_level}
        eval "declare initializer=\${${literal_transitions_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for literal_id in "${transitions[@]}"; do
            if [[ -v "descriptions[$literal_id]" ]]; then
                completions_trailing_space+=("${literals[$literal_id]}")
                suffixes_trailing_space+=("${literals[$literal_id]}")
                descriptions_trailing_space+=("${descriptions[$literal_id]}")
            else
                completions_no_description_trailing_space+=("${literals[$literal_id]}")
            fi
        done

        declare subword_transitions_name=subword_transitions_level_${fallback_level}
        eval "declare initializer=\${${subword_transitions_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for subword_id in "${transitions[@]}"; do
            _awmsg_subword_${subword_id} complete "${words[$CURRENT]}"
            completions_no_description_trailing_space+=("${subword_completions_no_description_trailing_space[@]}")
            completions_trailing_space+=("${subword_completions_trailing_space[@]}")
            completions_no_trailing_space+=("${subword_completions_no_trailing_space[@]}")
            suffixes_no_trailing_space+=("${subword_suffixes_no_trailing_space[@]}")
            suffixes_trailing_space+=("${subword_suffixes_trailing_space[@]}")
            descriptions_trailing_space+=("${subword_descriptions_trailing_space[@]}")
            descriptions_no_trailing_space+=("${subword_descriptions_no_trailing_space[@]}")
        done

        declare commands_name=commands_level_${fallback_level}
        eval "declare initializer=\${${commands_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for command_id in "${transitions[@]}"; do
            local output=$(_awmsg_cmd_${command_id} "${words[$CURRENT]}")
            local -a command_completions=("${(@f)output}")
            for line in ${command_completions[@]}; do
                local parts=(${(@s:	:)line})
                if [[ -v "parts[2]" ]]; then
                    completions_trailing_space+=("${parts[1]}")
                    suffixes_trailing_space+=("${parts[1]}")
                    descriptions_trailing_space+=("${parts[2]}")
                else
                    completions_no_description_trailing_space+=("${parts[1]}")
                fi
            done
        done

        declare specialized_commands_name=specialized_commands_level_${fallback_level}
        eval "declare initializer=\${${specialized_commands_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for command_id in "${transitions[@]}"; do
            _awmsg_cmd_${command_id} ${words[$CURRENT]}
        done

        local maxlen=0
        for suffix in ${suffixes_trailing_space[@]}; do
            if [[ ${#suffix} -gt $maxlen ]]; then
                maxlen=${#suffix}
            fi
        done
        for suffix in ${suffixes_no_trailing_space[@]}; do
            if [[ ${#suffix} -gt $maxlen ]]; then
                maxlen=${#suffix}
            fi
        done

        for ((i = 1; i <= $#suffixes_trailing_space; i++)); do
            if [[ -z ${descriptions_trailing_space[$i]} ]]; then
                descriptions_trailing_space[$i]="${(r($maxlen)( ))${suffixes_trailing_space[$i]}}"
            else
                descriptions_trailing_space[$i]="${(r($maxlen)( ))${suffixes_trailing_space[$i]}} -- ${descriptions_trailing_space[$i]}"
            fi
        done

        for ((i = 1; i <= $#suffixes_no_trailing_space; i++)); do
            if [[ -z ${descriptions_no_trailing_space[$i]} ]]; then
                descriptions_no_trailing_space[$i]="${(r($maxlen)( ))${suffixes_no_trailing_space[$i]}}"
            else
                descriptions_no_trailing_space[$i]="${(r($maxlen)( ))${suffixes_no_trailing_space[$i]}} -- ${descriptions_no_trailing_space[$i]}"
            fi
        done

        compadd -O m -a completions_no_description_trailing_space; matches+=("${m[@]}")
        compadd -O m -a completions_no_description_no_trailing_space; matches+=("${m[@]}")
        compadd -O m -a completions_trailing_space; matches+=("${m[@]}")
        compadd -O m -a completions_no_trailing_space; matches+=("${m[@]}")

        if [[ ${#matches} -gt 0 ]]; then
            compadd -Q -a completions_no_description_trailing_space
            compadd -Q -S ' ' -a completions_no_description_no_trailing_space
            compadd -l -Q -a -d descriptions_trailing_space completions_trailing_space
            compadd -l -Q -S '' -a -d descriptions_no_trailing_space completions_no_trailing_space
            return 0
        fi
    }
}

if [[ $ZSH_EVAL_CONTEXT =~ :file$ ]]; then
    compdef _awmsg awmsg
else
    _awmsg
fi
