/**
 * This class stores the handlers for the individual checkboxes available in the developer overlay.
 * Such a class must have label and onPress function.
 * If the class has a checked property, then that will be called every simulation update to
 * synchronize the state of the checkbox (only if the developer overaly is opened).
 */
class DeveloperOverlayControlCheckboxes
{
}

DeveloperOverlayControlCheckboxes.prototype.ControlAll = class
{
	label()
	{
		return translate("Control all units");
	}

	onPress(checked)
	{
		Engine.PostNetworkCommand({
			"type": "control-all",
			"flag": checked
		});
	}

	checked()
	{
		const playerState = g_SimState.players[g_ViewedPlayer];
		return playerState ? playerState.controlsAll : false;
	}
};

DeveloperOverlayControlCheckboxes.prototype.ChangePerspective = class
{
	constructor(playerViewControl)
	{
		this.playerViewControl = playerViewControl;
	}

	label()
	{
		return translate("Change perspective");
	}

	onPress(checked)
	{
		this.playerViewControl.setChangePerspective(checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.SelectionEntityState = class
{
	constructor(playerViewControl, selection)
	{
		this.developerOverlayEntityState = new DeveloperOverlayEntityState(selection);
	}

	label()
	{
		return translate("Display selection state");
	}

	onPress(checked)
	{
		this.developerOverlayEntityState.setEnabled(checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.PathfinderOverlay = class
{
	label()
	{
		return translate("Pathfinder overlay");
	}

	onPress(checked)
	{
		Engine.GuiInterfaceCall("SetPathfinderDebugOverlay", checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.HierarchicalPathfinderOverlay = class
{
	label()
	{
		return translate("Hierarchical pathfinder overlay");
	}

	onPress(checked)
	{
		Engine.GuiInterfaceCall("SetPathfinderHierDebugOverlay", checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.ObstructionOverlay = class
{
	label()
	{
		return translate("Obstruction overlay");
	}

	onPress(checked)
	{
		Engine.GuiInterfaceCall("SetObstructionDebugOverlay", checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.UnitMotionOverlay = class
{
	label()
	{
		return translate("Unit motion overlay");
	}

	onPress(checked)
	{
		g_Selection.SetMotionDebugOverlay(checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.RangeOverlay = class
{
	label()
	{
		return translate("Range overlay");
	}

	onPress(checked)
	{
		Engine.GuiInterfaceCall("SetRangeDebugOverlay", checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.BoundingBoxOverlay = class
{
	label()
	{
		return translate("Bounding box overlay");
	}

	onPress(checked)
	{
		Engine.SetBoundingBoxDebugOverlay(checked);
	}
};

DeveloperOverlayControlCheckboxes.prototype.RestrictCamera = class
{
	label()
	{
		return translate("Restrict camera");
	}

	onPress(checked)
	{
		Engine.GameView_SetConstrainCameraEnabled(checked);
	}

	checked()
	{
		return Engine.GameView_GetConstrainCameraEnabled();
	}
};

DeveloperOverlayControlCheckboxes.prototype.RevealMap = class
{
	label()
	{
		return translate("Reveal map");
	}

	onPress(checked)
	{
		Engine.PostNetworkCommand({
			"type": "reveal-map",
			"enable": checked
		});
	}

	checked()
	{
		return Engine.GuiInterfaceCall("IsMapRevealed");
	}
};

DeveloperOverlayControlCheckboxes.prototype.EnableTimeWarp = class
{
	constructor()
	{
		this.timeWarp = new TimeWarp();
	}

	label()
	{
		return translate("Enable time warp");
	}

	onPress(checked)
	{
		this.timeWarp.setEnabled(checked);
	}
};


DeveloperOverlayControlCheckboxes.prototype.ActivateRejoinTest = class
{
	constructor()
	{
		this.disabled = false;
	}

	label()
	{
		return translate("Activate Rejoin Test");
	}

	onPress(checked)
	{
		const box = new SessionMessageBox();
		box.Title = "Rejoin Test";
		box.Caption = "Warning: the rejoin test can't be de-activated and is quite slow. Its only purpose is to check for OOS.";
		const self = this;
		box.Buttons = [
			{ "caption": "Cancel" }, { "caption": "OK", "onPress": () => {
				Engine.ActivateRejoinTest();
				this.disabled = true;
				this.update();
			} }
		];
		box.display();
	}

	checked()
	{
		return this.disabled;
	}

	enabled()
	{
		return !this.disabled && g_InitAttributes.mapType != "random";
	}
};

DeveloperOverlayControlCheckboxes.prototype.PromoteSelectedUnits = class
{
	label()
	{
		return translate("Promote selected units");
	}

	onPress(checked)
	{
		Engine.PostNetworkCommand({
			"type": "promote",
			"entities": g_Selection.toList()
		});
	}

	checked()
	{
		return false;
	}
};

DeveloperOverlayControlCheckboxes.prototype.EnableCulling = class
{
	label()
	{
		return translate("Enable culling");
	}

	onPress(checked)
	{
		Engine.GameView_SetCullingEnabled(checked);
	}

	checked()
	{
		return Engine.GameView_GetCullingEnabled();
	}
};

DeveloperOverlayControlCheckboxes.prototype.LockCullCamera = class
{
	label()
	{
		return translate("Lock cull camera");
	}

	onPress(checked)
	{
		Engine.GameView_SetLockCullCameraEnabled(checked);
	}

	checked()
	{
		return Engine.GameView_GetLockCullCameraEnabled();
	}
};

DeveloperOverlayControlCheckboxes.prototype.DisplayCameraFrustum = class
{
	label()
	{
		return translate("Display camera frustum");
	}

	onPress(checked)
	{
		Engine.Renderer_SetDisplayFrustumEnabled(checked);
	}

	checked()
	{
		return Engine.Renderer_GetDisplayFrustumEnabled();
	}
};

DeveloperOverlayControlCheckboxes.prototype.DisplayShadowsFrustum = class
{
	label()
	{
		return translate("Display shadows frustum");
	}

	onPress(checked)
	{
		Engine.Renderer_SetDisplayShadowsFrustumEnabled(checked);
	}

	checked()
	{
		return Engine.Renderer_GetDisplayShadowsFrustumEnabled();
	}
};
