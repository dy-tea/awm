#compdef awmsg

_awmsg_cmd_0 () {
    IPREFIX="$2" PREFIX="$1" _path_files
}

compadd_intercept () {
    declare is_filter_array=false
    declare output_array=""
    while getopts "12aACDdEefFiIJ:klM:nO:o:p:P:QqrRs:S:UW:Xx" opt; do
        case "$opt" in
            D) is_filter_array=1;;
            O) output_array=$OPTARG;;
        esac
    done
    if [[ "$is_filter_array" == true || "$output_array" != "" ]]; then
        prev_compadd "$@"
        return $?
    fi
    builtin compadd -O complgen_matches "$@"
    prev_compadd "$@"
}

_awmsg () {
    declare -a literals=(-h --help -v --version exit spawn -c --continuous -1 --1-line -s --socket output list toplevels modes create destroy workspace list set toplevel list focused keyboard list device list current bind list run none maximize fullscreen previous next move up down left right close swap_up swap_down swap_left swap_right half_up half_down half_left half_right tile tile_sans auto_tile open window_to display rule list)
    declare -A descrs=()
    descrs[0]="show help"
    descrs[1]="show version"
    descrs[2]="exit awm"
    descrs[3]="spawn a command"
    descrs[4]="keep writing updates until cancelled"
    descrs[5]="write on a single line"
    descrs[6]="use socket path"
    descrs[7]="list outputs"
    descrs[8]="list toplevels on outputs"
    descrs[9]="list output modes"
    descrs[10]="create an output with WIDTHxHEIGHT"
    descrs[11]="destroy an output with the given name"
    descrs[12]="list workspaces"
    descrs[13]="set current workspace to num"
    descrs[14]="list toplevels"
    descrs[15]="show focused toplevel info"
    descrs[16]="list keyboards"
    descrs[17]="list devices"
    descrs[18]="show current device"
    descrs[19]="list key bindings"
    descrs[20]="run key binding for name"
    descrs[21]="do nothing"
    descrs[22]="maximize the active window"
    descrs[23]="fullscreen the active window"
    descrs[24]="focus the previous window"
    descrs[25]="focus the next window"
    descrs[26]="start an interactive move with the active window"
    descrs[27]="focus the window in the up direction"
    descrs[28]="focus the window in the down direction"
    descrs[29]="focus the window in the left direction"
    descrs[30]="focus the window in the right direction"
    descrs[31]="close the active window"
    descrs[32]="swap the active window with the window in the up direction"
    descrs[33]="swap the active window with the window in the down direction"
    descrs[34]="swap the active window with the window in the left direction"
    descrs[35]="swap the active window with the window in the right direction"
    descrs[36]="half the active window in the up direction"
    descrs[37]="half the active window in the down direction"
    descrs[38]="half the active window in the left direction"
    descrs[39]="half the active window in the right direction"
    descrs[40]="tile all windows in the active workspace"
    descrs[41]="tile all windows in the active workspace excluding the active one"
    descrs[42]="toggle automatic tiling for the active workspace"
    descrs[43]="focus workspace N"
    descrs[44]="move the active window to workspace N"
    descrs[45]="display key binding for name"
    descrs[46]="list windowrules"
    declare -A descr_id_from_literal_id=([1]=0 [2]=0 [3]=1 [4]=1 [5]=2 [6]=3 [7]=4 [8]=4 [9]=5 [10]=5 [11]=6 [12]=6 [14]=7 [15]=8 [16]=9 [17]=10 [18]=11 [20]=12 [21]=13 [23]=14 [24]=15 [26]=16 [28]=17 [29]=18 [31]=19 [32]=20 [33]=21 [34]=22 [35]=23 [36]=24 [37]=25 [38]=26 [39]=27 [40]=28 [41]=29 [42]=30 [43]=31 [44]=32 [45]=33 [46]=34 [47]=35 [48]=36 [49]=37 [50]=38 [51]=39 [52]=40 [53]=41 [54]=42 [55]=43 [56]=44 [57]=45 [59]=46)
    declare -A literal_transitions=()
    literal_transitions[1]="([1]=2 [2]=2 [3]=2 [4]=2 [5]=2 [6]=3 [7]=4 [8]=4 [9]=4 [10]=4 [11]=5 [12]=5 [13]=6 [19]=7 [22]=8 [25]=9 [27]=10 [30]=11 [58]=12)"
    literal_transitions[4]="([7]=4 [8]=4 [9]=4 [10]=4 [11]=5 [12]=5 [13]=6 [19]=7 [22]=8 [25]=9 [27]=10 [30]=11 [58]=12)"
    literal_transitions[6]="([14]=2 [15]=2 [16]=2 [17]=3 [18]=3)"
    literal_transitions[7]="([20]=2 [21]=3)"
    literal_transitions[8]="([23]=2 [24]=2)"
    literal_transitions[9]="([26]=2)"
    literal_transitions[10]="([28]=2 [29]=2)"
    literal_transitions[11]="([31]=2 [32]=13 [57]=13)"
    literal_transitions[12]="([59]=2)"
    literal_transitions[13]="([5]=2 [33]=2 [34]=2 [35]=2 [36]=2 [37]=2 [38]=2 [39]=2 [40]=2 [41]=2 [42]=2 [43]=2 [44]=2 [45]=2 [46]=2 [47]=2 [48]=2 [49]=2 [50]=2 [51]=2 [52]=2 [53]=2 [54]=2 [55]=3 [56]=3)"
    declare -A star_transitions=([3]=2)

    declare state=1
    declare word_index=2
    while [[ $word_index -lt $CURRENT ]]; do
        declare word=${words[$word_index]}

        if [[ -v "literal_transitions[$state]" ]]; then
            eval "declare -A state_transitions=${literal_transitions[$state]}"

            declare word_matched=0
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

        if [[ -v "star_transitions[$state]" ]]; then
            state=${star_transitions[$state]}
            word_index=$((word_index + 1))
            continue
        fi

        return 1
    done

    declare -A literal_transitions_level_0=([10]="28 29" [1]="1 2 3 4 5 6 7 8 9 10 11 12 13 19 22 25 27 30 58" [4]="7 8 9 10 11 12 13 19 22 25 27 30 58" [9]="26" [11]="31 32 57" [12]="59" [6]="14 15 16 17 18" [13]="5 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56" [8]="23 24" [7]="20 21")
    declare -A commands_level_0=()
    declare -A compadd_commands_level_0=([5]="0")

    declare max_fallback_level=0
    for (( fallback_level=0; fallback_level <= max_fallback_level; fallback_level++ )); do
        completions_no_description_trailing_space=()
        completions_no_description_no_trailing_space=()
        completions_trailing_space=()
        suffixes_trailing_space=()
        descriptions_trailing_space=()
        completions_no_trailing_space=()
        suffixes_no_trailing_space=()
        descriptions_no_trailing_space=()
        matches=()
        compadd_matches_count=0
        declare literal_transitions_name=literal_transitions_level_${fallback_level}
        eval "declare initializer=\${${literal_transitions_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for literal_id in "${transitions[@]}"; do
            if [[ -v "descr_id_from_literal_id[$literal_id]" ]]; then
                declare descr_id=$descr_id_from_literal_id[$literal_id]
                completions_trailing_space+=("${literals[$literal_id]}")
                suffixes_trailing_space+=("${literals[$literal_id]}")
                descriptions_trailing_space+=("${descrs[$descr_id]}")
            else
                completions_no_description_trailing_space+=("${literals[$literal_id]}")
            fi
        done
        declare commands_name=commands_level_${fallback_level}
        eval "declare initializer=\${${commands_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for command_id in "${transitions[@]}"; do
            declare output=$(_awmsg_cmd_${command_id} "${words[$CURRENT]}")
            declare -a command_completions=("${(@f)output}")
            for line in ${command_completions[@]}; do
                declare parts=(${(@s:	:)line})
                if [[ -v "parts[2]" ]]; then
                    completions_trailing_space+=("${parts[1]}")
                    suffixes_trailing_space+=("${parts[1]}")
                    descriptions_trailing_space+=("${parts[2]}")
                else
                    completions_no_description_trailing_space+=("${parts[1]}")
                fi
            done
        done
        declare compadd_commands_name=compadd_commands_level_${fallback_level}
        eval "declare initializer=\${${compadd_commands_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        declare compadd_type_string=$(type -w compadd)
        declare compadd_type_fields=(${(z)compadd_type_string})
        declare compadd_type=${compadd_type_fields[2]}
        if [[ "$compadd_type" == builtin ]]; then
            function prev_compadd () {
                builtin compadd "$@"
            }
        else
            functions -c compadd prev_compadd
        fi
        functions -c compadd_intercept compadd
        for command_id in "${transitions[@]}"; do
            declare -a complgen_matches=()
            _awmsg_cmd_${command_id} ${words[$CURRENT]} ""
            (( compadd_matches_count += ${#complgen_matches} ))
        done
        if [[ "$compadd_type" == builtin ]]; then
            unfunction compadd
        else
            functions -c prev_compadd compadd
            unfunction prev_compadd
        fi
        declare maxlen=0
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
        if [[ $compadd_matches_count -gt 0 || ${#matches} -gt 0 ]]; then
            compadd -Q -a completions_no_description_trailing_space
            compadd -Q -S ' ' -a completions_no_description_no_trailing_space
            compadd -l -Q -a -d descriptions_trailing_space completions_trailing_space
            compadd -l -Q -S '' -a -d descriptions_no_trailing_space completions_no_trailing_space
            return 0
        fi
    done
}

if [[ $ZSH_EVAL_CONTEXT =~ :file$ ]]; then
    compdef _awmsg awmsg
else
    _awmsg
fi
