const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");

const window = {};
const emittedScript = fs.readFileSync(process.argv[2], "utf8");
vm.runInNewContext(emittedScript, { window });

assert.equal(window.PrismaL10N.locale, "es");
assert.equal(window.PrismaL10N.messages.t, "T");
assert.equal(window.PrismaL10N.messages.__proto__, "Proto");
assert.equal(window.PrismaL10N.t("greeting", { name: "Ada" }), "Hola Ada");
assert.equal(window.PrismaL10N.t("count", { n: 0, empty: "" }), "0/");
assert.equal(window.PrismaL10N.t("greeting"), "Hola {{name}}");
assert.equal(window.PrismaL10N.t("braces", {}), "literal {x} and {{missing}}");
assert.equal(window.PrismaL10N.t("unicode", { name: "世界" }), "Café 世界");
assert.equal(window.PrismaL10N.t("missing.key"), "missing.key");

console.log("V4 localization JavaScript behavior passed");
