export function isTrustedDockMessage(event, expectedSource, expectedOrigin) {
    return event.source === expectedSource && event.origin === expectedOrigin;
}
