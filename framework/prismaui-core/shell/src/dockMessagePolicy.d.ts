export function isTrustedDockMessage(
    event: Pick<MessageEvent, "source" | "origin">,
    expectedSource: MessageEventSource | null,
    expectedOrigin: string,
): boolean;
