(function () {
  "use strict";

  var TEXT_TYPES = ["", "text", "search", "password", "email", "url", "tel", "number"];

  function tagOf(node) {
    return node && node.tagName ? String(node.tagName).toUpperCase() : "";
  }

  function isTextEntryTarget(node) {
    var tag = tagOf(node);
    if (tag === "TEXTAREA") return true;
    if (tag === "INPUT") return TEXT_TYPES.indexOf(String(node.type || "").toLowerCase()) >= 0;
    return !!(node && node.isContentEditable);
  }

  function isSelectControl(node) {
    return tagOf(node) === "SELECT";
  }

  function isFormEntryTarget(node) {
    return isTextEntryTarget(node) || isSelectControl(node);
  }

  window.PrismaInput = {
    isTextEntryTarget: isTextEntryTarget,
    isSelectControl: isSelectControl,
    isFormEntryTarget: isFormEntryTarget,
    isKeyboardCaptureTarget: function (node) {
      return isFormEntryTarget(node && node.tagName ? node : document.activeElement);
    }
  };
})();
