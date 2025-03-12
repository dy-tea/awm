#compdef awmsg

_awmsg () {
    local -a literals=("help" "exit" "spawn" "-c" "--continuous" "output" "list" "modes" "workspace" "list" "set" "toplevel" "list" "keyboard" "list" "device" "list" "current" "bind" "list" "run" "display")

    local -A descriptions
    descriptions[1]="show help"
    descriptions[2]="exit awm"
    descriptions[3]="spawn a command"
    descriptions[4]="keep reading events until cancelled"
    descriptions[5]="keep reading events until cancelled"
    descriptions[7]="list outputs"
    descriptions[8]="list output modes"
    descriptions[10]="list workspaces"
    descriptions[11]="set current workspace to num"
    descriptions[13]="list toplevels"
    descriptions[15]="list keyboards"
    descriptions[17]="list devices"
    descriptions[18]="show current device"
    descriptions[20]="list key bindings"
    descriptions[21]="run key binding for name"
    descriptions[22]="display key binding for name"

    local -A literal_transitions
    literal_transitions[1]="([1]=2 [2]=2 [3]=3 [4]=4 [5]=4 [6]=5 [9]=6 [12]=7 [14]=8 [16]=9 [19]=10)"
    literal_transitions[4]="([4]=4 [5]=4 [6]=5 [9]=6 [12]=7 [14]=8 [16]=9 [19]=10)"
    literal_transitions[5]="([7]=2 [8]=2)"
    literal_transitions[6]="([10]=2 [11]=12)"
    literal_transitions[7]="([13]=2)"
    literal_transitions[8]="([15]=2)"
    literal_transitions[9]="([17]=2 [18]=2)"
    literal_transitions[10]="([20]=2 [21]=11 [22]=11)"

    local -A match_anything_transitions
    match_anything_transitions=([12]=2 [11]=2 [3]=2)

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
    declare -A literal_transitions_level_0=([7]="13" [8]="15" [1]="1 2 3 4 5 6 9 12 14 16 19" [9]="17 18" [4]="4 5 6 9 12 14 16 19" [10]="20 21 22" [6]="10 11" [5]="7 8")
    declare -A subword_transitions_level_0=()
    declare -A commands_level_0=()
    declare -A specialized_commands_level_0=()

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
