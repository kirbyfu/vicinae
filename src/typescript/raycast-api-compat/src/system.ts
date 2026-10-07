import * as vicinae from "@vicinae/api";
import type { PathLike } from "fs";

export type RaycastApplication = {
	name: string;
	path: string;
	bundleId: string;
	localizedName: string;
	windowsAppId: string;
};

export const transformApp = (app: vicinae.Application): RaycastApplication => {
	return {
		bundleId: app.id,
		name: app.name,
		path: app.path,
		localizedName: app.name,
		windowsAppId: app.id,
	};
};

// accepts both raycast-shaped apps (from getApplications) and vicinae ones (appPicker preference values)
export const open = async (
	target: string,
	application?: string | Partial<RaycastApplication & vicinae.Application>,
) => {
	const app =
		typeof application === "string"
			? application
			: (application?.id ?? application?.bundleId ?? application?.windowsAppId);

	await vicinae.open(target, app);
};

export const getApplications = async (
	path?: PathLike,
): Promise<RaycastApplication[]> => {
	const apps = await vicinae.getApplications(path?.toString());

	return apps.map<RaycastApplication>(transformApp);
};

export const getDefaultApplication = async (
	path: PathLike,
): Promise<RaycastApplication> => {
	return transformApp(await vicinae.getDefaultApplication(path.toString()));
};

export const getFrontmostApplication =
	async (): Promise<RaycastApplication> => {
		return transformApp(await vicinae.getFrontmostApplication());
	};

export const captureException = (exception: unknown): void => {
	// maybe one day, if we have a developer hub...
	console.error("captureException called on", exception);
};

export const showInFinder = (path: PathLike) =>
	vicinae.showInFileBrowser(path, { select: true });

export const getSelectedFinderItems = async (): Promise<{ path: string }[]> => {
	throw new Error("getSelectedFinderItems is not implemented");
};
