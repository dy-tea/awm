function _awmsg_cmd_0
    set 1 $argv[1]
    __fish_complete_path "$1"
end

function _awmsg_subword
    set mode $argv[1]
    set word $argv[2]

    set char_index 1
    set matched 0
    while true
        if test $char_index -gt (string length -- "$word")
            set matched 1
            break
        end

        set subword (string sub --start=$char_index -- "$word")

        if set --query subword_literal_transitions_inputs[$subword_state] && test -n $subword_literal_transitions_inputs[$subword_state]
            set inputs (string split ' ' $subword_literal_transitions_inputs[$subword_state])
            set tos (string split ' ' $subword_literal_transitions_tos[$subword_state])

            set literal_matched 0
            for literal_id in (seq 1 (count $subword_literals))
                set literal $subword_literals[$literal_id]
                set literal_len (string length -- "$literal")
                set subword_slice (string sub --end=$literal_len -- "$subword")
                if test $subword_slice = $literal
                    set index (contains --index -- "$literal_id" $inputs)
                    set subword_state $tos[$index]
                    set char_index (math $char_index + $literal_len)
                    set literal_matched 1
                    break
                end
            end
            if test $literal_matched -ne 0
                continue
            end
        end

        set index (contains --index -- "$subword_state" $subword_match_anything_transitions_from)
        if test -n "$index"
            set subword_state $subword_match_anything_transitions_to[$index]
            set matched 1
            break
        end

        break
    end

    if test $mode = matches
        return (math 1 - $matched)
    end

    set unmatched_suffix (string sub --start=$char_index -- $word)

    set matched_prefix
    if test $char_index -eq 1
        set matched_prefix ""
    else
        set matched_prefix (string sub --end=(math $char_index - 1) -- "$word")
    end

    for fallback_level in (seq 0 $subword_max_fallback_level)
        set candidates
        set froms_name subword_literal_transitions_from_level_$fallback_level
        set froms (string split ' ' $$froms_name)
        if contains $subword_state $froms
            set index (contains --index -- "$subword_state" $froms)
            set transitions_name subword_literal_transitions_level_$fallback_level
            printf 'set transitions (string split \' \' $%s[%d])' $transitions_name $index | source
            for literal_id in $transitions
                set unmatched_suffix_len (string length -- $unmatched_suffix)
                if test $unmatched_suffix_len -gt 0
                    set literal $subword_literals[$literal_id]
                    set slice (string sub --end=$unmatched_suffix_len -- $literal)
                    if test "$slice" != "$unmatched_suffix"
                        continue
                    end
                end
                if test -n "$subword_descriptions[$literal_id]"
                    set --append candidates (printf '%s%s\t%s\n' $matched_prefix $subword_literals[$literal_id] $subword_descriptions[$literal_id])
                else
                    set --append candidates (printf '%s%s\n' $matched_prefix $subword_literals[$literal_id])
                end
            end
        end

        set froms_name subword_commands_from_level_$fallback_level
        set froms (string split ' ' $$froms_name)
        set index (contains --index -- "$subword_state" $froms)
        if test -n "$index"
            printf 'set function_id $subword_commands_level_%s[%d]' $fallback_level $index | source
            set function_name _awmsg_cmd_$function_id
            $function_name "$matched_prefix" | while read line
                set --append candidates (printf '%s%s\n' $matched_prefix $line)
            end
        end

        printf '%s\n' $candidates | __complgen_match && break
    end
end

function _awmsg_subword_1
    set mode $argv[1]
    set word $argv[2]

    set --global subword_literals x

    set --global subword_descriptions

    set --global subword_literal_transitions_inputs
    set --global subword_literal_transitions_inputs[2] 1
    set --global subword_literal_transitions_tos[2] 3

    set --global subword_match_anything_transitions_from 1 3
    set --global subword_match_anything_transitions_to 2 4

    set --global subword_literal_transitions_from_level_0 2
    set --global subword_literal_transitions_level_0 1
    set --global subword_commands_from_level_0 
    set --global subword_commands_level_0 
    set --global subword_max_fallback_level 0

    set --global subword_state 1
    _awmsg_subword "$mode" "$word"
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

    set literals -h --help -v --version exit spawn -c --continuous -1 --1-line -s --socket output list toplevels modes create destroy workspace list set toplevel list keyboard list device list current bind list run none maximize fullscreen previous next move up down left right close swap_up swap_down swap_left swap_right half_up half_down half_left half_right tile tile_sans open window_to display rule list

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
    set descriptions[15] "list toplevels on outputs"
    set descriptions[16] "list output modes"
    set descriptions[17] "create an output with the given width and height"
    set descriptions[18] "destroy an output with the given name"
    set descriptions[20] "list workspaces"
    set descriptions[21] "set current workspace to num"
    set descriptions[23] "list toplevels"
    set descriptions[25] "list keyboards"
    set descriptions[27] "list devices"
    set descriptions[28] "show current device"
    set descriptions[30] "list key bindings"
    set descriptions[31] "run key binding for name"
    set descriptions[32] "do nothing"
    set descriptions[33] "maximize the active window"
    set descriptions[34] "fullscreen the active window"
    set descriptions[35] "focus the previous window"
    set descriptions[36] "focus the next window"
    set descriptions[37] "start an interactive move with the active window"
    set descriptions[38] "focus the window in the up direction"
    set descriptions[39] "focus the window in the down direction"
    set descriptions[40] "focus the window in the left direction"
    set descriptions[41] "focus the window in the right direction"
    set descriptions[42] "close the active window"
    set descriptions[43] "swap the active window with the window in the up direction"
    set descriptions[44] "swap the active window with the window in the down direction"
    set descriptions[45] "swap the active window with the window in the left direction"
    set descriptions[46] "swap the active window with the window in the right direction"
    set descriptions[47] "half the active window in the up direction"
    set descriptions[48] "half the active window in the down direction"
    set descriptions[49] "half the active window in the left direction"
    set descriptions[50] "half the active window in the right direction"
    set descriptions[51] "tile all windows in the active workspace"
    set descriptions[52] "tile all windows in the active workspace excluding the active one"
    set descriptions[53] "focus workspace N"
    set descriptions[54] "move the active window to workspace N"
    set descriptions[55] "display key binding for name"
    set descriptions[57] "list windowrules"

    set literal_transitions_inputs
    set literal_transitions_inputs[1] "1 2 3 4 5 6 7 8 9 10 11 12 13 19 22 24 26 29 56"
    set literal_transitions_tos[1] "2 2 2 2 2 3 4 4 4 4 5 5 6 7 8 9 10 11 12"
    set literal_transitions_inputs[4] "7 8 9 10 11 12 13 19 22 24 26 29 56"
    set literal_transitions_tos[4] "4 4 4 4 5 5 6 7 8 9 10 11 12"
    set literal_transitions_inputs[6] "14 15 16 17 18"
    set literal_transitions_tos[6] "2 2 2 15 16"
    set literal_transitions_inputs[7] "20 21"
    set literal_transitions_tos[7] "2 17"
    set literal_transitions_inputs[8] 23
    set literal_transitions_tos[8] 2
    set literal_transitions_inputs[9] 25
    set literal_transitions_tos[9] 2
    set literal_transitions_inputs[10] "27 28"
    set literal_transitions_tos[10] "2 2"
    set literal_transitions_inputs[11] "30 31 55"
    set literal_transitions_tos[11] "2 13 13"
    set literal_transitions_inputs[12] 57
    set literal_transitions_tos[12] 2
    set literal_transitions_inputs[13] "5 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54"
    set literal_transitions_tos[13] "2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 14 14"

    set match_anything_transitions_from 5 3 14 16 17
    set match_anything_transitions_to 4 2 2 2 2
    set subword_transitions_ids[15] 1
    set subword_transitions_tos[15] 2

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

        if set --query subword_transitions_ids[$state] && test -n $subword_transitions_ids[$state]
            set subword_ids (string split ' ' $subword_transitions_ids[$state])
            set tos $subword_transitions_tos[$state]

            set subword_matched 0
            for subword_id in $subword_ids
                if _awmsg_subword_$subword_id matches "$word"
                    set subword_matched 1
                    set state $tos[$subword_id]
                    set word_index (math $word_index + 1)
                    break
                end
            end
            if test $subword_matched -ne 0
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

    set literal_froms_level_0 12 7 8 1 4 9 10 6 11 13
    set literal_inputs_level_0 "57|20 21|23|1 2 3 4 5 6 7 8 9 10 11 12 13 19 22 24 26 29 56|7 8 9 10 11 12 13 19 22 24 26 29 56|25|27 28|14 15 16 17 18|30 31 55|5 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54"
    set subword_froms_level_0 15
    set subwords_level_0 "1"
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

        set subwords_name subword_froms_level_$fallback_level
        set subwords $$subwords_name
        set index (contains --index -- "$state" $subwords)
        if test -n "$index"
            set subwords_name subwords_level_$fallback_level
            set subwords (string split ' ' $$subwords_name)
            for id in $subwords
                set function_name _awmsg_subword_$id
                set --append candidates ($function_name complete "$COMP_WORDS[$COMP_CWORD]")
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
