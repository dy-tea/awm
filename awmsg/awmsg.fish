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

    set literals -h --help -v --version exit spawn -c --continuous -1 --1-line -s --socket output list toplevels modes create destroy workspace list set toplevel list focused keyboard list device list current bind list run none maximize fullscreen previous next move resize pin toggle_floating up down left right close swap_up swap_down swap_left swap_right half_up half_down half_left half_right tile tile_sans auto_tile open window_to display rule list

    set descrs
    set descrs[1] "show help"
    set descrs[2] "show version"
    set descrs[3] "exit awm"
    set descrs[4] "spawn a command"
    set descrs[5] "keep writing updates until cancelled"
    set descrs[6] "write on a single line"
    set descrs[7] "use socket path"
    set descrs[8] "list outputs"
    set descrs[9] "list toplevels on outputs"
    set descrs[10] "list output modes"
    set descrs[11] "create an output with WIDTHxHEIGHT"
    set descrs[12] "destroy an output with the given name"
    set descrs[13] "list workspaces"
    set descrs[14] "set current workspace to num"
    set descrs[15] "list toplevels"
    set descrs[16] "show focused toplevel info"
    set descrs[17] "list keyboards"
    set descrs[18] "list devices"
    set descrs[19] "show current device"
    set descrs[20] "list key bindings"
    set descrs[21] "run key binding for name"
    set descrs[22] "do nothing"
    set descrs[23] "maximize the active window"
    set descrs[24] "fullscreen the active window"
    set descrs[25] "focus the previous window"
    set descrs[26] "focus the next window"
    set descrs[27] "start an interactive move with the active window"
    set descrs[28] "start an interactive resize with the active window"
    set descrs[29] "pin/unpin the active window"
    set descrs[30] "toggle floating/tiling state for the active window"
    set descrs[31] "focus the window in the up direction"
    set descrs[32] "focus the window in the down direction"
    set descrs[33] "focus the window in the left direction"
    set descrs[34] "focus the window in the right direction"
    set descrs[35] "close the active window"
    set descrs[36] "swap the active window with the window in the up direction"
    set descrs[37] "swap the active window with the window in the down direction"
    set descrs[38] "swap the active window with the window in the left direction"
    set descrs[39] "swap the active window with the window in the right direction"
    set descrs[40] "half the active window in the up direction"
    set descrs[41] "half the active window in the down direction"
    set descrs[42] "half the active window in the left direction"
    set descrs[43] "half the active window in the right direction"
    set descrs[44] "tile all windows in the active workspace"
    set descrs[45] "tile all windows in the active workspace excluding the active one"
    set descrs[46] "toggle automatic tiling for the active workspace"
    set descrs[47] "focus workspace N"
    set descrs[48] "move the active window to workspace N"
    set descrs[49] "display key binding for name"
    set descrs[50] "list windowrules"
    set descr_literal_ids 1 2 3 4 5 6 7 8 9 10 11 12 14 15 16 17 18 20 21 23 24 26 28 29 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 62
    set descr_ids 1 1 2 2 3 4 5 5 6 6 7 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50
    set regexes 
    set literal_transitions_inputs
    set literal_transitions_inputs[1] "1 2 3 4 5 6 7 8 9 10 11 12 13 19 22 25 27 30 61"
    set literal_transitions_tos[1] "2 2 2 2 2 3 4 4 4 4 5 5 6 7 8 9 10 11 12"
    set literal_transitions_inputs[4] "7 8 9 10 11 12 13 19 22 25 27 30 61"
    set literal_transitions_tos[4] "4 4 4 4 5 5 6 7 8 9 10 11 12"
    set literal_transitions_inputs[6] "14 15 16 17 18"
    set literal_transitions_tos[6] "2 2 2 3 3"
    set literal_transitions_inputs[7] "20 21"
    set literal_transitions_tos[7] "2 3"
    set literal_transitions_inputs[8] "23 24"
    set literal_transitions_tos[8] "2 2"
    set literal_transitions_inputs[9] 26
    set literal_transitions_tos[9] 2
    set literal_transitions_inputs[10] "28 29"
    set literal_transitions_tos[10] "2 2"
    set literal_transitions_inputs[11] "31 32 60"
    set literal_transitions_tos[11] "2 13 13"
    set literal_transitions_inputs[12] 62
    set literal_transitions_tos[12] 2
    set literal_transitions_inputs[13] "5 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59"
    set literal_transitions_tos[13] "2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 3 3"

    set star_transitions_from 3
    set star_transitions_to 2

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

        if set --query star_transitions_from[$state] && test -n $star_transitions_from[$state]
            set index (contains --index -- "$state" $star_transitions_from)
            set state $star_transitions_to[$index]
            set word_index (math $word_index + 1)
            continue
        end

        return 1
    end

    set literal_froms_level_0 10 1 4 9 11 12 6 13 8 7
    set literal_inputs_level_0 "28 29|1 2 3 4 5 6 7 8 9 10 11 12 13 19 22 25 27 30 61|7 8 9 10 11 12 13 19 22 25 27 30 61|26|31 32 60|62|14 15 16 17 18|5 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59|23 24|20 21"
    set command_froms_level_0 5
    set commands_level_0 "0"

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
                set descr_index (contains --index -- "$literal_id" $descr_literal_ids)
                if test -n "$descr_index"
                    set --append candidates (printf '%s\t%s\n' $literals[$literal_id] $descrs[$descr_ids[$descr_index]])
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
            set --append candidates ($function_name "$COMP_WORDS[$COMP_CWORD]" "")
        end
        printf '%s\n' $candidates | __complgen_match $COMP_WORDS[$word_index] && return 0
    end
end

complete --erase awmsg
complete --command awmsg --no-files --arguments "(_awmsg)"
