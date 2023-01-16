using Wings;

namespace WingsExample {
    static class Extensions {
        public static int FindLastNotOf(this string str, params char[] chars) {
            for (int i = str.Length - 1; i >= 0; i--) {
                if (!chars.Contains(str[i])) {
                    return i;
                }
            }
            return -1;
        }
    }

    static class Program {
        static void Main(string[] args) {
            Wg.DefaultConfig(out var cfg);
            cfg.enableOSAccess = true;
            cfg.argv = args;
            cfg.argc = args.Length;
            cfg.importPath = AppDomain.CurrentDomain.BaseDirectory;

            Wg.Context context = Wg.CreateContext(cfg);

            Wg.Obj sysexit = Wg.GetGlobal(context, "SystemExit");
            Wg.IncRef(sysexit);

            // This context is only used to check if strings
            // are expressions rather than a set of statements.
            Wg.Context exprChecker = Wg.CreateContext();

            Console.WriteLine("Wings C# Example REPL");

            string input = string.Empty;
            bool indented = false;
            while (true) {
                if (input.Length == 0) {
                    Console.Write(">>> ");
                } else {
                    Console.Write("... ");
                }

                string line = Console.ReadLine() ?? string.Empty;
                input += line + "\n";

                int lastCharIndex = line.FindLastNotOf(' ', '\t');
                if (lastCharIndex != -1 && line[lastCharIndex] == ':') {
                    indented = true;
                    continue;
                }

                if (indented && line.Length > 0) {
                    continue;
                }

                Wg.Obj result = new();
                Wg.ClearException(exprChecker);
                if (Wg.CompileExpression(exprChecker, input)) {
                    result = Wg.ExecuteExpression(context, input, "<string>");
                } else {
                    Wg.Execute(context, input, "<string>");
                }
                input = string.Empty;
                indented = false;

                if (result && !Wg.IsNone(result)) {
                    Wg.Obj repr = Wg.UnaryOp(Wg.UnOp.REPR, result);
                    if (repr) {
                        Console.WriteLine(Wg.GetString(repr, out int _));
                    }
                }

                Wg.Obj exc = Wg.GetException(context);
                if (exc) {
                    if (Wg.IsInstance(exc, new[] { sysexit }, 1)) {
                        break;
                    }

                    Console.WriteLine(Wg.GetErrorMessage(context));
                    Wg.ClearException(context);
                }
            }

            Wg.DestroyContext(exprChecker);
            Wg.DestroyContext(context);
        }
    }
}
