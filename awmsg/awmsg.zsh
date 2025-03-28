#compdef awmsg

_awmsg () {
    local -a literals=("help" "exit" "spawn" "-h" "--help" "-c" "--continuous" "-1" "--1-line" "output" "list" "modes" "workspace" "list" "set" "toplevel" "list" "keyboard" "list" "device" "list" "current" "bind" "list" "run" "display")

    local -A descriptions
    descriptions[1]="show help"
    descriptions[2]="exit awm"
    descriptions[3]="spawn a command"
    descriptions[4]="show help"
    descriptions[5]="show help"
    descriptions[6]="keep writing updates until cancelled"
    descriptions[7]="keep writing updates until cancelled"
    descriptions[8]="write on a single line"
    descriptions[9]="write on a single line"
    descriptions[11]="list outputs"
    descriptions[12]="list output modes"
    descriptions[14]="list workspaces"
    descriptions[15]="set current workspace to num"
    descriptions[17]="list toplevels"
    descriptions[19]="list keyboards"
    descriptions[21]="list devices"
    descriptions[22]="show current device"
    descriptions[24]="list key bindings"
    descriptions[25]="run key binding for name"
    descriptions[26]="display key binding for name"

    local -A literal_transitions
    literal_transitions[1]="([1]=2 [2]=2 [3]=3 [4]=4 [5]=4 [6]=4 [7]=4 [8]=4 [9]=4 [10]=5 [13]=6 [16]=7 [18]=8 [20]=9 [23]=10)"
    literal_transitions[4]="([4]=4 [5]=4 [6]=4 [7]=4 [8]=4 [9]=4 [10]=5 [13]=6 [16]=7 [18]=8 [20]=9 [23]=10)"
    literal_transitions[5]="([11]=2 [12]=2)"
    literal_transitions[6]="([14]=2 [15]=12)"
    literal_transitions[7]="([17]=2)"
    literal_transitions[8]="([19]=2)"
    literal_transitions[9]="([21]=2 [22]=2)"
    literal_transitions[10]="([24]=2 [25]=11 [26]=11)"

    local -A match_anything_transitions
    match_anything_transitions=([11]=2 [12]=2 [3]=2)

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
    declare -A literal_transitions_level_0=([7]="17" [8]="19" [1]="1 2 3 4 5 6 7 8 9 10 13 16 18 20 23" [9]="21 22" [4]="4 5 6 7 8 9 10 13 16 18 20 23" [10]="24 25 26" [6]="14 15" [5]="11 12")
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
