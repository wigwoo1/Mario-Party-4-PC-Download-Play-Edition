static int state = 7;
static int zero_state;
static int *relocated_pointer = &state;
int read_state(void) { return *relocated_pointer + zero_state; }
void write_state(int value) { state = value - 5; zero_state = 5; }
