// Whist Compiler — Self-hosted
// Port of w0/main.c command-line option processing.

import std;
import fs;
import lexer;
use lexer::*;

struct MainOptions {
    lex_only: bool,
    parse_only: bool,
    check_only: bool,
    print_ast: bool,
    print_ast_checked: bool,
    rc_debug: bool,
    emit_c: bool,
    line_directives: bool,
    compile_only: bool,
    output_file: Option<string>,
    lib_path: Option<string>,
}

// --- Stub functions ---
// Each matches a w0 action entry point. They print what they would do
// and return 0 (success).

func run_lex_mode(source_file: string) -> i32 {
    var source = fs::read_file(source_file);
    if (source == "") {
        std::eprintln($"Could not open file: {source_file}");
        return 1;
    }

    std::println("Source:");
    std::println(source);
    std::println("Tokens:");
    std::println("LINE  TYPE          VALUE");
    std::println("----  ------------  -----");

    var lex = new Lexer(source);

    while (true) {
        var tok = lex.next();
        var line_str = std::to_string(tok.line).pad_left(3, ' ');
        var col_str = std::to_string(tok.column).pad_right(2, ' ');
        var type_str = tok.kind.name().pad_right(12, ' ');

        if (tok.kind == TokenType::EOF) {
            std::println($"{line_str}:{col_str}  {type_str}");
            break;
        } else {
            std::println($"{line_str}:{col_str}  {type_str}  {tok.value}");
        }
    }

    return 0;
}

func run_debug_pipeline(opts: MainOptions, source_file: string) -> i32 {
    std::println($"[stub] run_debug_pipeline: {source_file}");
    if (opts.parse_only) {
        std::println("  mode: parse only");
    } else if (opts.check_only) {
        std::println("  mode: check only");
    }
    if (opts.print_ast) {
        std::println("  flag: print AST");
    }
    if (opts.print_ast_checked) {
        std::println("  flag: print checked AST");
    }
    return 0;
}

func run_normal_compile(opts: MainOptions, source_file: string) -> i32 {
    std::println($"[stub] run_normal_compile: {source_file}");
    if (opts.output_file is Some(path)) {
        std::println($"  output: {path}");
    } else {
        std::println("  output: stdout");
    }
    return 0;
}

func compile_to_object(opts: MainOptions, source_file: string) -> i32 {
    var out = opts.output_file.value_or("<stdout>");
    std::println($"[stub] compile_to_object: {source_file} -> {out}");
    return 0;
}

func compile_and_link(opts: MainOptions, input_files: Vec<string>) -> i32 {
    var out = opts.output_file.value_or("<stdout>");
    std::println($"[stub] compile_and_link: {input_files.count} file(s) -> {out}");
    foreach (const f in input_files) {
        std::println($"  input: {f}");
    }
    return 0;
}

func compile_and_run(opts: MainOptions, source_file: string, run_args: Vec<string>) -> i32 {
    std::println($"[stub] compile_and_run: {source_file}");
    if (run_args.count > 0) {
        std::println($"  args: {run_args.count} forwarded arg(s)");
    }
    return 0;
}

func compile_and_test(opts: MainOptions, source_file: string) -> i32 {
    std::println($"[stub] compile_and_test: {source_file}");
    return 0;
}

// --- Option helpers ---

func is_option_with_value(arg: string) -> bool {
    return arg == "--lib-path" || arg == "-o";
}

func print_usage(program: string) {
    std::eprintln($"Usage: {program} [options] <source.w>");
    std::eprintln($"       {program} -c <source.w> -o <output.o> [options]");
    std::eprintln($"       {program} <inputs...> -o <output> [options]");
    std::eprintln($"       {program} run [options] <source-file> [args...]");
    std::eprintln($"       {program} test [options] <source-file>");
    std::eprintln("Options:");
    std::eprintln("  -c        Compile to object file (requires -o)");
    std::eprintln("  --lex     Lex only (print tokens)");
    std::eprintln("  --parse   Parse only (no type checking)");
    std::eprintln("  --check   Type check only (no code generation)");
    std::eprintln("  --ast     Print AST");
    std::eprintln("  --ast-checked");
    std::eprintln("            Print AST with checker annotations");
    std::eprintln("  --lib-path <dir>");
    std::eprintln("            Library search path for module imports");
    std::eprintln("  --rc-debug");
    std::eprintln("            Emit RC tracking debug output to stderr");
    std::eprintln("  --emit-c  Dump generated C to stdout (works with run/test/-c)");
    std::eprintln("  --line-directives");
    std::eprintln("            Emit #line directives in generated C");
    std::eprintln("  -o <file> Output file");
    std::eprintln("Commands:");
    std::eprintln("  run       Compile and run the program");
    std::eprintln("  test      Compile and run tests");
    std::eprintln("Inputs can be .w source files and/or .o object files.");
    std::eprintln("With -c: compile one .w to .o (separate compilation).");
    std::eprintln("Without -c: compile .w files and link all .o files into executable.");
}

// --- Option parsing ---

// First pass: extract global options that affect all modes.
func prescan_global_options(args: Vec<string>) -> MainOptions {
    var no_string: Option<string> = Option::None;
    var opts = new MainOptions{
        lex_only: false,
        parse_only: false,
        check_only: false,
        print_ast: false,
        print_ast_checked: false,
        rc_debug: false,
        emit_c: false,
        line_directives: false,
        compile_only: false,
        output_file: no_string,
        lib_path: no_string,
    };

    var i: i64 = 1;
    while (i < args.count) {
        if (args[i] == "--lib-path" && i + 1 < args.count) {
            i += 1;
            opts.lib_path = Option::Some(args[i]);
        } else if (args[i] == "-o" && i + 1 < args.count) {
            i += 1;
            opts.output_file = Option::Some(args[i]);
        } else if (args[i] == "--rc-debug") {
            opts.rc_debug = true;
        } else if (args[i] == "--emit-c") {
            opts.emit_c = true;
        } else if (args[i] == "--line-directives") {
            opts.line_directives = true;
        } else if (args[i] == "-c") {
            opts.compile_only = true;
        }
        i += 1;
    }

    return opts;
}

// Find the index of a subcommand ("run" or "test"), skipping options.
func find_subcommand_index(args: Vec<string>, subcommand: string) -> i64 {
    var i: i64 = 1;
    while (i < args.count) {
        if (args[i] == subcommand) {
            return i;
        }
        if (is_option_with_value(args[i]) && i + 1 < args.count) {
            i += 1;
        }
        i += 1;
    }
    return -1;
}

// Find the source file after a subcommand, skipping options.
// Returns (source_path, arg_start_index), or ("", -1) if not found.
func find_subcommand_source(args: Vec<string>, subcommand_idx: i64) -> (string, i64) {
    var i = subcommand_idx + 1;
    while (i < args.count) {
        if (args[i] == "--lib-path" && i + 1 < args.count) {
            i += 1;
        } else if (args[i] == "--rc-debug" || args[i] == "--emit-c" || args[i] == "--line-directives") {
            // skip flag
        } else {
            return (args[i], i);
        }
        i += 1;
    }
    return ("", -1);
}

// Try to handle "run" or "test" subcommands.
// Returns -1 if no subcommand found, otherwise returns the exit code.
func try_handle_subcommand(args: Vec<string>, opts: MainOptions) -> i32 {
    var run_idx = find_subcommand_index(args, "run");
    if (run_idx >= 0) {
        var (run_source, arg_start) = find_subcommand_source(args, run_idx);
        if (run_source == "") {
            std::eprintln($"Usage: {args[0]} run [options] <source-file> [args...]");
            return 1;
        }
        var run_args = new Vec<string>{};
        // Collect args after the source file
        var i = arg_start + 1;
        while (i < args.count) {
            run_args.push(args[i]);
            i += 1;
        }
        return compile_and_run(opts, run_source, run_args);
    }

    var test_idx = find_subcommand_index(args, "test");
    if (test_idx >= 0) {
        var (test_source, _) = find_subcommand_source(args, test_idx);
        if (test_source == "") {
            std::eprintln($"Usage: {args[0]} test [options] <source-file>");
            return 1;
        }
        return compile_and_test(opts, test_source);
    }

    return -1;
}

// Second pass: parse debug/pipeline flags from the front of the argument list.
// Returns the index of the first non-option argument.
func parse_main_options(args: Vec<string>, opts: MainOptions) -> i64 {
    var i: i64 = 1;
    while (i < args.count && args[i].starts_with("-")) {
        if (args[i] == "--lex") {
            opts.lex_only = true;
        } else if (args[i] == "--parse") {
            opts.parse_only = true;
        } else if (args[i] == "--check") {
            opts.check_only = true;
        } else if (args[i] == "--ast") {
            opts.print_ast = true;
        } else if (args[i] == "--ast-checked") {
            opts.print_ast_checked = true;
        } else if (args[i] == "--lib-path" && i + 1 < args.count) {
            i += 1;
            opts.lib_path = Option::Some(args[i]);
        } else if (args[i] == "--rc-debug") {
            opts.rc_debug = true;
        } else if (args[i] == "--emit-c") {
            opts.emit_c = true;
        } else if (args[i] == "--line-directives") {
            opts.line_directives = true;
        } else if (args[i] == "-c") {
            opts.compile_only = true;
        } else if (args[i] == "-o" && i + 1 < args.count) {
            i += 1;
            opts.output_file = Option::Some(args[i]);
        }
        i += 1;
    }
    return i;
}

// Collect positional arguments (input files), skipping options.
func collect_input_files(args: Vec<string>, start: i64) -> Vec<string> {
    var files = new Vec<string>{};
    var i = start;
    while (i < args.count) {
        if (is_option_with_value(args[i]) && i + 1 < args.count) {
            i += 1; // skip option and its value
        } else if (args[i].starts_with("-")) {
            // skip standalone flags
        } else {
            files.push(args[i]);
        }
        i += 1;
    }
    return files;
}

// --- Main ---

func main() -> i32 {
    const args = std::args();

    var opts = prescan_global_options(args);
    if (opts.rc_debug) {
        opts.line_directives = true;
    }

    // Try subcommands first (run, test)
    var subcommand_result = try_handle_subcommand(args, opts);
    if (subcommand_result >= 0) {
        return subcommand_result;
    }

    // Full option parse for main mode
    var arg_idx = parse_main_options(args, opts);
    if (opts.rc_debug) {
        opts.line_directives = true;
    }

    if (arg_idx >= args.count) {
        print_usage(args[0]);
        return 1;
    }

    var input_files = collect_input_files(args, arg_idx);

    // -c mode: compile single .w to .o
    if (opts.compile_only) {
        if (input_files.count != 1 || !input_files[0].ends_with(".w")) {
            std::eprintln("Error: -c requires exactly one .w source file");
            return 1;
        }
        if (opts.output_file is None && !opts.emit_c) {
            std::eprintln("Error: -c requires -o <output.o> (or --emit-c)");
            return 1;
        }
        return compile_to_object(opts, input_files[0]);
    }

    // Multi-file / link mode: multiple inputs or any .o files
    var has_obj_input = input_files.any(|f| f.ends_with(".o"));
    if (input_files.count > 1 || has_obj_input) {
        if (opts.output_file is None) {
            std::eprintln("Error: -o <output> required when linking multiple files");
            return 1;
        }
        return compile_and_link(opts, input_files);
    }

    // Single .w file mode
    var source_file = input_files[0];

    if (opts.lex_only) {
        return run_lex_mode(source_file);
    } else if (!opts.parse_only && !opts.check_only && !opts.print_ast && !opts.print_ast_checked) {
        return run_normal_compile(opts, source_file);
    } else {
        return run_debug_pipeline(opts, source_file);
    }
}
