module tracker;

struct TrackedPlanesNode {
  string lat;
  string log;
  string fid;
  string name;
  int spd;
}

class IndexOpError : Exception {
  this(string msg = "cannot find index", string file = __FILE__, size_t line = __LINE__) {
    super(msg, file, line);
  }
}

class TrackedPlanes {
  import vibe.vibe;

  public {
    TrackedPlanesNode indexOp(string uuid) {
      foreach (plane; planes) {
        if (plane.fid == uuid) {
          return plane;
        }
      }

      throw new IndexOpError;
    }

    void updateOrAdd(string lat, string log, string fid, string name, int spd) {
      foreach (ref plane; planes) {
        if (plane.fid == fid) {
          plane.lat = lat; plane.log = log; plane.name = name; plane.spd = spd;
          return;
        }
      }
      planes ~= TrackedPlanesNode(lat, log, fid, name, spd);
      logInfo("Added flight " ~ fid);
    }

    TrackedPlanesNode[] planes;
  }
}
