#compdef awmsg

_awmsg_cmd_0 () {
    _path_files
}

_awmsg_subword () {
    local mode=$1
    local word=$2


    local char_index=0
    local matched=0
    while true; do
        if [[ $char_index -ge ${#word} ]]; then
            matched=1
            break
        fi

        local subword=${word:$char_index}

        if [[ -v "subword_literal_transitions[$subword_state]" ]]; then
            declare -A state_transitions
            eval "state_transitions=${subword_literal_transitions[$subword_state]}"

            local literal_matched=0
            for ((literal_id = 1; literal_id <= $#subword_literals; literal_id++)); do
                local literal=${subword_literals[$literal_id]}
                local literal_len=${#literal}
                if [[ ${subword:0:$literal_len} = "$literal" ]]; then
                    if [[ -v "state_transitions[$literal_id]" ]]; then
                        subword_state=${state_transitions[$literal_id]}
                        char_index=$((char_index + literal_len))
                        literal_matched=1
                    fi
                fi
            done
            if [[ $literal_matched -ne 0 ]]; then
                continue
            fi
        fi

        if [[ -v "subword_match_anything_transitions[$subword_state]" ]]; then
            subword_state=${subword_match_anything_transitions[$subword_state]}
            matched=1
            break
        fi

        break
    done

    if [[ $mode = matches ]]; then
        return $((1 - matched))
    fi

    local matched_prefix="${word:0:$char_index}"
    local completed_prefix="${word:$char_index}"

    subword_completions_no_description_trailing_space=()
    subword_completions_trailing_space=()
    subword_completions_no_trailing_space=()
    subword_suffixes_trailing_space=()
    subword_suffixes_no_trailing_space=()
    subword_descriptions_trailing_space=()
    subword_descriptions_no_trailing_space=()

    for (( subword_fallback_level=0; subword_fallback_level <= subword_max_fallback_level; subword_fallback_level++ )) {
         declare literal_transitions_name=subword_literal_transitions_level_${subword_fallback_level}
         eval "declare initializer=\${${literal_transitions_name}[$subword_state]}"
         eval "declare -a transitions=($initializer)"
         for literal_id in "${transitions[@]}"; do
             local literal=${subword_literals[$literal_id]}
             if [[ $literal = "${completed_prefix}"* ]]; then
                 local completion="$matched_prefix$literal"
                 if [[ -v "subword_descriptions[$literal_id]" ]]; then
                     subword_completions_no_trailing_space+=("${completion}")
                     subword_suffixes_no_trailing_space+=("${completion}")
                     subword_descriptions_no_trailing_space+=("${subword_descriptions[$literal_id]}")
                 else
                     subword_completions_no_trailing_space+=("${completion}")
                     subword_suffixes_no_trailing_space+=("${literal}")
                     subword_descriptions_no_trailing_space+=('')
                 fi
             fi
         done

         declare commands_name=subword_commands_level_${subword_fallback_level}
         eval "declare initializer=\${${commands_name}[$subword_state]}"
         eval "declare -a transitions=($initializer)"
         for command_id in "${transitions[@]}"; do
             local candidates=()
             local output=$(_awmsg_cmd_${command_id} "$matched_prefix")
             local -a command_completions=("${(@f)output}")
             for line in ${command_completions[@]}; do
                 if [[ $line = "${completed_prefix}"* ]]; then
                     local parts=(${(@s:	:)line})
                     if [[ -v "parts[2]" ]]; then
                         local completion=$matched_prefix${parts[1]}
                         subword_completions_trailing_space+=("${completion}")
                         subword_suffixes_trailing_space+=("${parts[1]}")
                         subword_descriptions_trailing_space+=("${parts[2]}")
                     else
                         line="$matched_prefix$line"
                         subword_completions_no_description_trailing_space+=("$line")
                     fi
                 fi
             done
         done

         declare specialized_commands_name=subword_specialized_commands_level_${subword_fallback_level}
         eval "declare initializer=\${${specialized_commands_name}[$subword_state]}"
         eval "declare -a transitions=($initializer)"
         for command_id in "${transitions[@]}"; do
             local output=$(_awmsg_cmd_${command_id} "$matched_prefix")
             local -a candidates=("${(@f)output}")
             for line in ${candidates[@]}; do
                 if [[ $line = "${completed_prefix}"* ]]; then
                     line="$matched_prefix$line"
                     local parts=(${(@s:	:)line})
                     if [[ -v "parts[2]" ]]; then
                         subword_completions_trailing_space+=("${parts[1]}")
                         subword_suffixes_trailing_space+=("${parts[1]}")
                         subword_descriptions_trailing_space+=("${parts[2]}")
                     else
                         subword_completions_no_description_trailing_space+=("$line")
                     fi
                 fi
             done
         done

         if [[ ${#subword_completions_no_description_trailing_space} -gt 0 || ${#subword_completions_trailing_space} -gt 0 || ${#subword_completions_no_trailing_space} -gt 0 ]]; then
             break
         fi
    }
    return 0
}

_awmsg_subword_1 () {
    local -a subword_literals=("x")

    local -A subword_descriptions

    local -A subword_literal_transitions
    subword_literal_transitions[2]="([1]=3)"

    local -A subword_match_anything_transitions
    subword_match_anything_transitions=([1]=2 [3]=4)
    declare -A subword_literal_transitions_level_0=([2]="1")
    declare -A subword_commands_level_0=()
    declare -A subword_specialized_commands_level_0=()
    declare subword_max_fallback_level=0
    declare subword_state=1
    _awmsg_subword "$@"
}

_awmsg () {
    local -a literals=("-h" "--help" "-v" "--version" "exit" "spawn" "-c" "--continuous" "-1" "--1-line" "-s" "--socket" "output" "list" "toplevels" "modes" "create" "destroy" "workspace" "list" "set" "toplevel" "list" "keyboard" "list" "device" "list" "current" "bind" "list" "run" "none" "maximize" "fullscreen" "previous" "next" "move" "up" "down" "left" "right" "close" "swap_up" "swap_down" "swap_left" "swap_right" "half_up" "half_down" "half_left" "half_right" "tile" "tile_sans" "open" "window_to" "display" "rule" "list")

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
    descriptions[15]="list toplevels on outputs"
    descriptions[16]="list output modes"
    descriptions[17]="create an output with the given width and height"
    descriptions[18]="destroy an output with the given name"
    descriptions[20]="list workspaces"
    descriptions[21]="set current workspace to num"
    descriptions[23]="list toplevels"
    descriptions[25]="list keyboards"
    descriptions[27]="list devices"
    descriptions[28]="show current device"
    descriptions[30]="list key bindings"
    descriptions[31]="run key binding for name"
    descriptions[32]="do nothing"
    descriptions[33]="maximize the active window"
    descriptions[34]="fullscreen the active window"
    descriptions[35]="focus the previous window"
    descriptions[36]="focus the next window"
    descriptions[37]="start an interactive move with the active window"
    descriptions[38]="focus the window in the up direction"
    descriptions[39]="focus the window in the down direction"
    descriptions[40]="focus the window in the left direction"
    descriptions[41]="focus the window in the right direction"
    descriptions[42]="close the active window"
    descriptions[43]="swap the active window with the window in the up direction"
    descriptions[44]="swap the active window with the window in the down direction"
    descriptions[45]="swap the active window with the window in the left direction"
    descriptions[46]="swap the active window with the window in the right direction"
    descriptions[47]="half the active window in the up direction"
    descriptions[48]="half the active window in the down direction"
    descriptions[49]="half the active window in the left direction"
    descriptions[50]="half the active window in the right direction"
    descriptions[51]="tile all windows in the active workspace"
    descriptions[52]="tile all windows in the active workspace excluding the active one"
    descriptions[53]="focus workspace N"
    descriptions[54]="move the active window to workspace N"
    descriptions[55]="display key binding for name"
    descriptions[57]="list windowrules"

    local -A literal_transitions
    literal_transitions[1]="([1]=2 [2]=2 [3]=2 [4]=2 [5]=2 [6]=3 [7]=4 [8]=4 [9]=4 [10]=4 [11]=5 [12]=5 [13]=6 [19]=7 [22]=8 [24]=9 [26]=10 [29]=11 [56]=12)"
    literal_transitions[4]="([7]=4 [8]=4 [9]=4 [10]=4 [11]=5 [12]=5 [13]=6 [19]=7 [22]=8 [24]=9 [26]=10 [29]=11 [56]=12)"
    literal_transitions[6]="([14]=2 [15]=2 [16]=2 [17]=15 [18]=16)"
    literal_transitions[7]="([20]=2 [21]=17)"
    literal_transitions[8]="([23]=2)"
    literal_transitions[9]="([25]=2)"
    literal_transitions[10]="([27]=2 [28]=2)"
    literal_transitions[11]="([30]=2 [31]=13 [55]=13)"
    literal_transitions[12]="([57]=2)"
    literal_transitions[13]="([5]=2 [32]=2 [33]=2 [34]=2 [35]=2 [36]=2 [37]=2 [38]=2 [39]=2 [40]=2 [41]=2 [42]=2 [43]=2 [44]=2 [45]=2 [46]=2 [47]=2 [48]=2 [49]=2 [50]=2 [51]=2 [52]=2 [53]=14 [54]=14)"

    local -A match_anything_transitions
    match_anything_transitions=([5]=4 [3]=2 [14]=2 [16]=2 [17]=2)

    declare -A subword_transitions
    subword_transitions[15]="([1]=2)"

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

        if [[ -v "subword_transitions[$state]" ]]; then
            declare -A state_transitions
            eval "state_transitions=${subword_transitions[$state]}"

            local subword_matched=0
            for subword_id in ${(k)state_transitions}; do
                if _awmsg_subword_"${subword_id}" matches "$word"; then
                    subword_matched=1
                    state=${state_transitions[$subword_id]}
                    word_index=$((word_index + 1))
                    break
                fi
            done
            if [[ $subword_matched -ne 0 ]]; then
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
    declare -A literal_transitions_level_0=([12]="57" [7]="20 21" [8]="23" [1]="1 2 3 4 5 6 7 8 9 10 11 12 13 19 22 24 26 29 56" [4]="7 8 9 10 11 12 13 19 22 24 26 29 56" [9]="25" [10]="27 28" [6]="14 15 16 17 18" [11]="30 31 55" [13]="5 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54")
    declare -A subword_transitions_level_0=([15]="1")
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
