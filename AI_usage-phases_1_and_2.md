# AI Usage Documentation

**Tool Used:** Gemini

## Task 1: Generating `parse_condition`

**1. Prompt given to AI:**
    "I am building a C program for an OS lab. I have records with a specific structure. 
    Please generate a function with this exact signature: `int parse_condition(const char *input, char *field, char *op, char *value);`. 
    It needs to take a string formatted exactly like 'field:operator:value' (for example 'severity:>=:2') and split it into its three respective parts. 
    It should return 1 on success and 0 on failure."

**2. What was generated:**
    The AI generated a concise function utilizing the `sscanf` function from the C standard library to parse the formatted string.

```c
int parse_condition(const char *input, char *field, char *op, char *value) {
    // Basic implementation splitting "field:op:value"
    return sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3;
}
```

**3. What I changed and why:**
    I largely kept the generated logic because `sscanf` is highly efficient for this specific delimiter pattern. 
However, when implementing it in my main code, I made sure to define field, op, and value with a strictly defined buffer size (MAX_STR of 64 bytes) before passing them into this function to prevent potential buffer overflows from malformed command-line inputs.

**4. What I learned:**
    I learned about the advanced format specifier %[^:] used in `sscanf`. While I previously used %s to read standard strings separated by spaces, adding the caret symbol inside brackets [^:] instructs the function to read all characters as a string until it encounters a colon. 
This is a very clean way to tokenize strings without needing to write a complex loop using `strtok`.

## Task 2: Generating `match_condition`

**1. Prompt given to AI:**
    "Here is my C struct for my project:"
```c
        typedef struct {
        int id;
        char inspector[64];
        float lat;
        float lon;
        char category[64];
        int severity;
        time_t timestamp;
        char description[256];
        } Report;
```

"Based on these fields and types, generate a function `int match_condition(Report *r, const char *field, const char *op, const char *value);`. 
It should return 1 if the record satisfies the condition passed in the string arguments, and 0 otherwise. 
Note that 'severity' is an integer, while 'category' is a string."


**2. What was generated:**
    The AI provided a function using `strcmp` to identify which field was being queried. 
    For the integer fields, it used `atoi()` to convert the value string.
    
```c
int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == val;
        if (strcmp(op, ">=") == 0) return r->severity >= val;
    } else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
    }
    return 0; 
}
```

**3. What I changed and why:**
    The AI's generated code was a good starting skeleton, but it was incomplete. 
    It only included the == and >= operators for the severity integer. 
    I had to manually expand the logic block to fully support all mathematical operators required by the spec (<, <=, >, !=). 
    I also verified that the default return at the very end of the function is 0, ensuring that if a user inputs an unsupported field or operator, the program safely ignores the record rather than falsely matching it.

**4. What I learned:**
    This exercise reinforced how C handles data types coming from command-line arguments argv. 
    Even if a user types a number like 2 for severity, it enters the program as a `char* string`    . 
    I learned that you must explicitly convert that string into the correct native C type (e.g., using atoi() for integers) before you can apply numerical comparison operators against the binary data stored in the struct. 
    You cannot evaluate r->severity >= "2".
