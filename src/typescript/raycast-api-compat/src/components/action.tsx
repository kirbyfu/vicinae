import * as vicinae from "@vicinae/api";
import type { ComponentProps, FC } from "react";
import { open } from "../system.js";

type OpenProps = Omit<vicinae.Action.Open.Props, "app"> & {
	application?: Parameters<typeof open>[1];
	onOpen?: (target: string) => void;
};

// raycast names the app prop `application` and reports the open through `onOpen`
const Open: FC<OpenProps> = ({ target, application, onOpen, ...props }) => (
	<vicinae.Action
		{...props}
		onAction={async () => {
			await open(target, application);
			onOpen?.(target);
		}}
	/>
);

export const Action = Object.assign(
	(props: ComponentProps<typeof vicinae.Action>) => (
		<vicinae.Action {...props} />
	),
	{ ...vicinae.Action, Open },
);
