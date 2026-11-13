import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const localEsbuild = path.resolve(here, "node_modules/esbuild/lib/main.js");
const sharedEsbuild = path.resolve(here, "../../LHolo/frontend/node_modules/esbuild/lib/main.js");
const esbuildPath = fs.existsSync(localEsbuild) ? localEsbuild : sharedEsbuild;
const esbuild = await import(pathToFileURL(esbuildPath).href);

const distDir = path.join(here, "dist");
fs.mkdirSync(distDir, { recursive: true });

await esbuild.build({
  entryPoints: [path.join(here, "src", "index.tsx")],
  bundle: true,
  format: "iife",
  platform: "browser",
  target: ["es2019"],
  outfile: path.join(distDir, "no-ad-servers.js"),
  jsx: "transform",
  jsxFactory: "__nasCreateElement",
  jsxFragment: "__nasFragment",
  banner: {
    js:
      "var __nasCreateElement=function(){var R=window.__NoAdServersRuntime.React;return R.createElement.apply(R,arguments);};" +
      "var __nasFragment=window.__NoAdServersRuntime.React.Fragment;"
  }
});

console.log("NoAdServers frontend built:", distDir);
