void loadDotEnvFile(string[] args) {
  import std.file : exists, readText;
  import vibe.core.log;
  import std.process : environment;
  import core.stdc.stdlib : exit;
  import std.conv : to;
  import std.string : splitLines;
  import std.array : split;

  logInfo("Loading enviroment variables");

  string dotenvFilePath = args.length == 2 ? args[1] : (exists(".env") ? ".env" : ".env.example");
  if (!exists(dotenvFilePath)) {
    logError(".env file not set correctly or does not exist.");
    exit(2);
  }

  string content = readText(dotenvFilePath);
  foreach (c, line; content.splitLines()) {
    if (line.length == 0 || line[0] == '#') continue;

    auto parts = line.split("=");
    if (parts.length != 2) {
      logError(dotenvFilePath ~ " could not be parsed: not enough args line ", to!string(c));
      exit(2);
    }

    logInfo("Loading " ~ parts[0] ~ ": " ~ parts[1]);
    environment[parts[0]] = parts[1];
  }
}
