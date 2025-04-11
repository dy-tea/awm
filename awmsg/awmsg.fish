function _awmsg_cmd_0
    set 1 $argv[1]
    __fish_complete_path "$1"
end

function __complgen_match
    set prefix $argv[1]

    set candidates
    set descriptions
    while read c
        set a (string split --max 1 -- "	" $c)
        set --append candidates $a[1]
        if set --query a[2]
            set --append descriptions $a[2]
        else
            set --append descriptions ""
        end
    end

    if test -z "$candidates"
        return 1
    end

    set escaped_prefix (string escape --style=regex -- $prefix)
    set regex "^$escaped_prefix.*"

    set matches_case_sensitive
    set descriptions_case_sensitive
    for i in (seq 1 (count $candidates))
        if string match --regex --quiet --entire -- $regex $candidates[$i]
            set --append matches_case_sensitive $candidates[$i]
            set --append descriptions_case_sensitive $descriptions[$i]
        end
    end

    if set --query matches_case_sensitive[1]
        for i in (seq 1 (count $matches_case_sensitive))
            printf '%s	%s\n' $matches_case_sensitive[$i] $descriptions_case_sensitive[$i]
        end
        return 0
    end

    set matches_case_insensitive
    set descriptions_case_insensitive
    for i in (seq 1 (count $candidates))
        if string match --regex --quiet --ignore-case --entire -- $regex $candidates[$i]
            set --append matches_case_insensitive $candidates[$i]
            set --append descriptions_case_insensitive $descriptions[$i]
        end
    end

    if set --query matches_case_insensitive[1]
        for i in (seq 1 (count $matches_case_insensitive))
            printf '%s	%s\n' $matches_case_insensitive[$i] $descriptions_case_insensitive[$i]
        end
        return 0
    end

    return 1
end


function _awmsg
    set COMP_LINE (commandline --cut-at-cursor)

    set COMP_WORDS
    echo $COMP_LINE | read --tokenize --array COMP_WORDS
    if string match --quiet --regex '.*\s$' $COMP_LINE
        set COMP_CWORD (math (count $COMP_WORDS) + 1)
    else
        set COMP_CWORD (count $COMP_WORDS)
    end

    set literals -h --help -v --version exit spawn -c --continuous -1 --1-line -s --socket output list modes workspace list set toplevel list keyboard list device list current bind list run display

    set descriptions
    set descriptions[1] "show help"
    set descriptions[2] "show help"
    set descriptions[3] "show version"
    set descriptions[4] "show version"
    set descriptions[5] "exit awm"
    set descriptions[6] "spawn a command"
    set descriptions[7] "keep writing updates until cancelled"
    set descriptions[8] "keep writing updates until cancelled"
    set descriptions[9] "write on a single line"
    set descriptions[10] "write on a single line"
    set descriptions[11] "use socket path"
    set descriptions[12] "use socket path"
    set descriptions[14] "list outputs"
    set descriptions[15] "list output modes"
    set descriptions[17] "list workspaces"
    set descriptions[18] "set current workspace to num"
    set descriptions[20] "list toplevels"
    set descriptions[22] "list keyboards"
    set descriptions[24] "list devices"
    set descriptions[25] "show current device"
    set descriptions[27] "list key bindings"
    set descriptions[28] "run key binding for name"
    set descriptions[29] "display key binding for name"

    set literal_transitions_inputs
    set literal_transitions_inputs[1] "1 2 3 4 5 6 7 8 9 10 11 12 13 16 19 21 23 26"
    set literal_transitions_tos[1] "2 2 2 2 2 3 4 4 4 4 5 5 6 7 8 9 10 11"
    set literal_transitions_inputs[4] "7 8 9 10 11 12 13 16 19 21 23 26"
    set literal_transitions_tos[4] "4 4 4 4 5 5 6 7 8 9 10 11"
    set literal_transitions_inputs[6] "14 15"
    set literal_transitions_tos[6] "2 2"
    set literal_transitions_inputs[7] "17 18"
    set literal_transitions_tos[7] "2 13"
    set literal_transitions_inputs[8] 20
    set literal_transitions_tos[8] 2
    set literal_transitions_inputs[9] 22
    set literal_transitions_tos[9] 2
    set literal_transitions_inputs[10] "24 25"
    set literal_transitions_tos[10] "2 2"
    set literal_transitions_inputs[11] "27 28 29"
    set literal_transitions_tos[11] "2 12 12"

    set match_anything_transitions_from 5 3 12 13
    set match_anything_transitions_to 4 2 2 2

    set state 1
    set word_index 2
    while test $word_index -lt $COMP_CWORD
        set -- word $COMP_WORDS[$word_index]

        if set --query literal_transitions_inputs[$state] && test -n $literal_transitions_inputs[$state]
            set inputs (string split ' ' $literal_transitions_inputs[$state])
            set tos (string split ' ' $literal_transitions_tos[$state])

            set literal_id (contains --index -- "$word" $literals)
            if test -n "$literal_id"
                set index (contains --index -- "$literal_id" $inputs)
                set state $tos[$index]
                set word_index (math $word_index + 1)
                continue
            end
        end

        if set --query match_anything_transitions_from[$state] && test -n $match_anything_transitions_from[$state]
            set index (contains --index -- "$state" $match_anything_transitions_from)
            set state $match_anything_transitions_to[$index]
            set word_index (math $word_index + 1)
            continue
        end

        return 1
    end

    set literal_froms_level_0 7 8 1 9 4 6 10 11
    set literal_inputs_level_0 "17 18|20|1 2 3 4 5 6 7 8 9 10 11 12 13 16 19 21 23 26|22|7 8 9 10 11 12 13 16 19 21 23 26|14 15|24 25|27 28 29"
    set command_froms_level_0 5
    set commands_level_0 "0"

    set max_fallback_level 0
    for fallback_level in (seq 0 0)
        set candidates
        set froms_name literal_froms_level_$fallback_level
        set froms $$froms_name
        set index (contains --index -- "$state" $froms)
        if test -n "$index"
            set level_inputs_name literal_inputs_level_$fallback_level
            set input_assoc_values (string split '|' $$level_inputs_name)
            set state_inputs (string split ' ' $input_assoc_values[$index])
            for literal_id in $state_inputs
                if test -n "$descriptions[$literal_id]"
                    set --append candidates (printf '%s\t%s\n' $literals[$literal_id] $descriptions[$literal_id])
                else
                    set --append candidates (printf '%s\n' $literals[$literal_id])
                end
            end
        end

        set commands_name command_froms_level_$fallback_level
        set commands $$commands_name
        set index (contains --index -- "$state" $commands)
        if test -n "$index"
            set commands_name commands_level_$fallback_level
            set commands (string split ' ' $$commands_name)
            set function_id $commands[$index]
            set function_name _awmsg_cmd_$function_id
            set --append candidates ($function_name "$COMP_WORDS[$COMP_CWORD]")
        end
        printf '%s\n' $candidates | __complgen_match $COMP_WORDS[$word_index] && return 0
    end
end

complete --erase awmsg
complete --command awmsg --no-files --arguments "(_awmsg)"
