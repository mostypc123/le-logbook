void main(string[] args) {
  import vibe.vibe;
  import dotenv;
  import std.process : environment;
  import std.conv : to;

  loadDotEnvFile(args);

  auto router = new URLRouter;
  router.get("/api/register", (req, res) {
    res.writeJsonBody(["msg": "hi"]);
  });

  auto settings = new HTTPServerSettings;
  settings.port = to!ushort(environment.get("PORT"));
  listenHTTP(settings, router);
  runApplication();
}
