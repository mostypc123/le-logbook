void main(string[] args) {
  import vibe.vibe;
  import dotenv;
  import std.process : environment;
  import std.conv : to;
  import tracker;

  loadDotEnvFile(args);

  auto tracked = new TrackedPlanes;

  auto router = new URLRouter;

  // i fucking hate all thsi fucking fullstack app dev fuckery
  router.any("*", (HTTPServerRequest req, HTTPServerResponse res) {
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.headers["Access-Control-Allow-Headers"] = "Content-Type";
    res.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
    if (req.method == HTTPMethod.OPTIONS) res.writeBody("", 204);
  });

  router.post("/tracker/update", (HTTPServerRequest req, HTTPServerResponse res) @trusted {
    auto json = req.json;
    try {
      auto fid = json["uuid"].get!string;
      auto pln = json["pln"].get!string;
      auto lat = json["lat"].get!string;
      auto log = json["log"].get!string;
      auto spd = json["spd"].get!int;

      tracked.updateOrAdd(lat, log, fid, pln, spd);

      logInfo("Updated " ~ fid);
    } catch (Exception e) {
      res.writeJsonBody(["status": "fail", "msg": e.msg]);
      return;
    }

    res.writeJsonBody(["status": "ok", "msg": "updated tracking data"]);
  });

  router.post("/tracker/get", (HTTPServerRequest req, HTTPServerResponse res) @trusted {
    auto json = req.json;

    string fid;

    try {
      fid = json["uuid"].get!string;

      auto p = tracked.indexOp(fid);

      res.writeJsonBody([
        "status": Json("ok"),
        "pln": Json(p.name),
        "lat": Json(p.lat),
        "log": Json(p.log),
        "spd": Json(p.spd)
      ]);
    } catch (IndexOpError) {
      res.writeJsonBody(["status": "fail", "msg": "no such plane " ~ fid]);
    } catch (Exception e) {
      res.writeJsonBody(["status": "fail", "msg": e.msg]);
    }
  });

  auto settings = new HTTPServerSettings;
  settings.port = to!ushort(environment.get("PORT"));
  listenHTTP(settings, router);
  runApplication();
}
