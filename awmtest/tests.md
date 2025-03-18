# Tests

### Dependencies

- `alacritty` (can be changed to any terminal emulator)
- `tmatrix` (this is to force updates so moving the cursor is not required)
- `waybar` (this is to test maximizing since it will change usable area)

### Usage

- Build the project with `meson setup build && ninja -C build`
- Run using `./build/awmtest`
- This will run awm with the default config multiple times, each time to test different features.
- On completion and passing of all tests the program will exit cleanly.
- On failure of a test the program will exit due to an assertion failure.

### Notes

- alacritty can be substitued for any terminal emulator that supports a startup command
- tmatrix is used to force updates and can be substituted for any program that will update the screen
- waybar is used to test maximizing, it should pass related tests regardless of your config if present
- The default config is used for all tests
