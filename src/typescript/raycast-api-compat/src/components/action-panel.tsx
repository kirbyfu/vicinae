import * as vicinae from "@vicinae/api";
import { Action } from "./action.js";

// older versions of the Raycast API had ActionPanel.Item
export const ActionPanel = Object.assign(vicinae.ActionPanel, {
	Item: Action,
});
