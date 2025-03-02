#include "Hooks.h"
#include "DirectionalMovementHandler.h"
#include "Offsets.h"
#include "Utils.h"
#include "WidgetHandler.h"

namespace Hooks
{
	struct SaveCamera
	{
		enum class RotationType : uint8_t
		{
			kNone,
			kFirstPerson,
			kThirdPerson,
			kHorse
		};
		RotationType rotationType = RotationType::kNone;
		RE::NiPoint2 rotation { 0.f, 0.f };
		bool bZoomSaved = false;
		float zoom = 0.f;

		void SaveX(float a_x, RotationType a_rotationType)
		{
			rotation.x = a_x;
			rotationType = a_rotationType;
		}

		void SaveY(float a_y, RotationType a_rotationType)
		{
			rotation.y = a_y;
			rotationType = a_rotationType;
		}

		void SaveXY(RE::NiPoint2 a_xy, RotationType a_rotationType)
		{
			rotation = a_xy;
			rotationType = a_rotationType;
		}

		float ConsumeX()
		{
			rotationType = RotationType::kNone;
			return rotation.x;
		}

		float ConsumeY()
		{
			rotationType = RotationType::kNone;
			return rotation.y;
		}

		RE::NiPoint2& ConsumeXY()
		{
			rotationType = RotationType::kNone;
			return rotation;
		}

		void SaveZoom(float a_zoom)
		{
			zoom = a_zoom;
			bZoomSaved = true;
		}

		float ConsumeZoom()
		{
			bZoomSaved = false;
			return zoom;
		}

	} savedCamera;

	void Install()
	{
		logger::trace("Hooking...");

		MovementHook::Hook();
		LookHook::Hook();
		FirstPersonStateHook::Hook();
		ThirdPersonStateHook::Hook();
		HorseCameraStateHook::Hook();
		TweenMenuCameraStateHook::Hook();
		VATSCameraStateHook::Hook();
		PlayerCameraTransitionStateHook::Hook();
		MovementHandlerAgentPlayerControlsHook::Hook();
		ProjectileHook::Hook();
		PlayerCharacterHook::Hook();
		PlayerControlsHook::Hook();
		AIProcess_SetRotationSpeedZHook::Hook();
		Actor_SetRotationHook::Hook();
		EnemyHealthHook::Hook();
		HeadtrackingHook::Hook();
		NukeSetIsNPCHook::Hook();
		PlayerCameraHook::Hook();
		MainUpdateHook::Hook();
		HorseAimHook::Hook();

		logger::trace("...success");
	}

	void MovementHook::ProcessThumbstick(RE::MovementHandler* a_this, RE::ThumbstickEvent* a_event, RE::PlayerControlsData* a_data)
	{
		bool bHandled = false;
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (a_event && directionalMovementHandler->IsFreeCamera() && a_event->IsLeft() && playerCharacter && !playerCharacter->IsOnMount())
		{
			RE::NiPoint2 inputDirection(a_event->xValue, a_event->yValue);
			bHandled = directionalMovementHandler->ProcessInput(inputDirection, a_data);
		}

		if (!bHandled)
		{
			_ProcessThumbstick(a_this, a_event, a_data);
		}
	}

	void MovementHook::ProcessButton(RE::MovementHandler* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
	{
		bool bHandled = false;
		DirectionalMovementHandler* directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		auto pressedDirections = &directionalMovementHandler->_pressedDirections;
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (a_event && directionalMovementHandler->IsFreeCamera() && playerCharacter && !playerCharacter->IsOnMount())
		{
			auto userEvent = a_event->QUserEvent();
			auto userEvents = RE::UserEvents::GetSingleton();

			bool bRelevant = false;

			RE::NiPoint2 inputDirection(0.f, 0.f);

			if (userEvent == userEvents->forward) {
				a_event->IsPressed() ? pressedDirections->set(DirectionalMovementHandler::Directions::kForward) : pressedDirections->reset(DirectionalMovementHandler::Directions::kForward);
				bRelevant = true;
			} else if (userEvent == userEvents->back) {
				a_event->IsPressed() ? pressedDirections->set(DirectionalMovementHandler::Directions::kBack) : pressedDirections->reset(DirectionalMovementHandler::Directions::kBack);
				bRelevant = true;
			} else if (userEvent == userEvents->strafeLeft) {
				a_event->IsPressed() ? pressedDirections->set(DirectionalMovementHandler::Directions::kLeft) : pressedDirections->reset(DirectionalMovementHandler::Directions::kLeft);
				bRelevant = true;
			} else if (userEvent == userEvents->strafeRight) {
				a_event->IsPressed() ? pressedDirections->set(DirectionalMovementHandler::Directions::kRight) : pressedDirections->reset(DirectionalMovementHandler::Directions::kRight);
				bRelevant = true;
			}

			if (bRelevant)
			{	
				if (pressedDirections->any(DirectionalMovementHandler::Directions::kForward)) {
					inputDirection.y += 1.f;
				}
				if (pressedDirections->any(DirectionalMovementHandler::Directions::kBack)) {
					inputDirection.y -= 1.f;
				}
				if (pressedDirections->any(DirectionalMovementHandler::Directions::kRight)) {
					inputDirection.x += 1.f;
				} 
				if (pressedDirections->any(DirectionalMovementHandler::Directions::kLeft)) {
					inputDirection.x -= 1.f;
				}

				bHandled = directionalMovementHandler->ProcessInput(inputDirection, a_data);
			}
		}

		if (!bHandled)
		{
			*pressedDirections = DirectionalMovementHandler::Directions::kInvalid;
			_ProcessButton(a_this, a_event, a_data);
		}
	}

	static bool bTargetRecentlySwitched;

	void LookHook::ProcessThumbstick(RE::LookHandler* a_this, RE::ThumbstickEvent* a_event, RE::PlayerControlsData* a_data)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		if (a_event && a_event->IsRight() && directionalMovementHandler->HasTargetLocked() && !directionalMovementHandler->ShouldFaceCrosshair()) 
		{
			if (!directionalMovementHandler->GetTargetLockUseRightThumbstick())
			{
				return;  // ensure lock camera movement during lockon
			}

			float absX = fabs(a_event->xValue);
			float absY = fabs(a_event->yValue);

			if (absX + absY > 0.1f && !bTargetRecentlySwitched) {
				if (absX > absY) {
					if (a_event->xValue > 0) {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kRight);
					} else {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kLeft);
					}
				} else {
					if (a_event->yValue > 0) {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kBack);
					} else {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kForward);
					}
				}

				bTargetRecentlySwitched = true;
			} 
			else if (absX + absY <= 0.1f)
			{
				bTargetRecentlySwitched = false;
			}
		}
		else
		{
			bTargetRecentlySwitched = false;
			_ProcessThumbstick(a_this, a_event, a_data);
		}
	}

	void LookHook::ProcessMouseMove(RE::LookHandler* a_this, RE::MouseMoveEvent* a_event, RE::PlayerControlsData* a_data)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		if (a_event && directionalMovementHandler->HasTargetLocked() && !directionalMovementHandler->ShouldFaceCrosshair())
		{
			if (!directionalMovementHandler->GetTargetLockUseMouse())
			{
				return; // ensure lock camera movement during lockon
			}

			int32_t absX = abs(a_event->mouseInputX);
			int32_t absY = abs(a_event->mouseInputY);

			if (absX + absY > 32)
			{
				if (absX > absY)
				{
					if (a_event->mouseInputX > 0) {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kRight);
					} else {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kLeft);
					}
				}
				else 
				{
					if (a_event->mouseInputY > 0) {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kBack);
					} else {
						directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kForward);
					}
				}

				bTargetRecentlySwitched = true;
			} 
			else if (absX + absY <= 32)
			{
				bTargetRecentlySwitched = false;
			}
		}
		else
		{
			bTargetRecentlySwitched = false;
			_ProcessMouseMove(a_this, a_event, a_data);
		}
	}

	void FirstPersonStateHook::OnEnterState(RE::FirstPersonState* a_this)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		if (directionalMovementHandler->GetFreeCameraEnabled()) {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (playerCharacter) {
				// turn character towards where the camera was looking in third person state before entering first person state
				if (savedCamera.rotationType == SaveCamera::RotationType::kThirdPerson) {
					auto x = savedCamera.ConsumeX();
					if (playerCharacter->actorState1.sitSleepState != RE::SIT_SLEEP_STATE::kIsSleeping) {  // don't do this while sleeping
						playerCharacter->SetRotationZ(x);
					}
				}
				savedCamera.bZoomSaved = false;

				directionalMovementHandler->ResetDesiredAngle();
			}
		}

		_OnEnterState(a_this);
	}

	void FirstPersonStateHook::OnExitState(RE::FirstPersonState* a_this)
	{
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		
		if (playerCharacter)
		{
			savedCamera.SaveX(playerCharacter->data.angle.z, SaveCamera::RotationType::kFirstPerson);
		}
		
		_OnExitState(a_this);
	}

	void FirstPersonStateHook::ProcessButton(RE::FirstPersonState* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		if (a_event && directionalMovementHandler->HasTargetLocked() && directionalMovementHandler->GetTargetLockUseScrollWheel())
		{
			auto userEvent = a_event->QUserEvent();
			auto userEvents = RE::UserEvents::GetSingleton();

			if (userEvent == userEvents->zoomIn) {
				directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kLeft);
				return;
			} else if (userEvent == userEvents->zoomOut) {
				directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kRight);
				return;
			}
		}

		_ProcessButton(a_this, a_event, a_data);
	}

	void ThirdPersonStateHook::OnEnterState(RE::ThirdPersonState* a_this)
	{
		_OnEnterState(a_this);

		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			if (savedCamera.rotationType == SaveCamera::RotationType::kHorse) {
				a_this->freeRotation.x = savedCamera.ConsumeX();
			}

			DirectionalMovementHandler::GetSingleton()->ResetDesiredAngle();
		}
	}

	void ThirdPersonStateHook::OnExitState(RE::ThirdPersonState* a_this)
	{
		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			RE::Actor* cameraTarget = nullptr;
			cameraTarget = static_cast<RE::PlayerCamera*>(a_this->camera)->cameraTarget.get().get();
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			
			RE::NiPoint2 rot = a_this->freeRotation;
			rot.x += playerCharacter->data.angle.z;
			rot.y += playerCharacter->data.angle.x;
			savedCamera.SaveXY(rot, SaveCamera::RotationType::kThirdPerson);
			savedCamera.SaveZoom(a_this->targetZoomOffset);
		}

		_OnExitState(a_this);
	}

	void ThirdPersonStateHook::SetFreeRotationMode(RE::ThirdPersonState* a_this, bool a_weaponSheathed)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();

		//directionalMovementHandler->Update();

		if (directionalMovementHandler->IsFreeCamera())
		{
			RE::Actor* cameraTarget = nullptr;
			cameraTarget = static_cast<RE::PlayerCamera*>(a_this->camera)->cameraTarget.get().get();

			bool bHasTargetLocked = directionalMovementHandler->HasTargetLocked();

			if (cameraTarget) {
				a_this->freeRotationEnabled = true;
				directionalMovementHandler->UpdateAIProcessRotationSpeed(cameraTarget);  // because the game is skipping the original call while in freecam

				if (!bHasTargetLocked) {
					float pitchDelta = -a_this->freeRotation.y;

					// swimming pitch fix and swim up/down buttons handling
					if (cameraTarget->IsSwimming()) {
						float mult = 1.f;
						float currentPitch = cameraTarget->data.angle.x;
						float desiredPitch = 0;

						if (directionalMovementHandler->_pressedDirections.any(DirectionalMovementHandler::Directions::kUp) || directionalMovementHandler->_pressedDirections.any(DirectionalMovementHandler::Directions::kDown)) {
							if (directionalMovementHandler->_pressedDirections.any(DirectionalMovementHandler::Directions::kUp)) {
								desiredPitch += directionalMovementHandler->HasMovementInput() ? -PI / 4 : -PI / 2;
							}
							if (directionalMovementHandler->_pressedDirections.any(DirectionalMovementHandler::Directions::kDown)) {
								desiredPitch += directionalMovementHandler->HasMovementInput() ? PI / 4 : PI / 2;
							}
							auto playerControls = RE::PlayerControls::GetSingleton();
							playerControls->data.moveInputVec = RE::NiPoint2(0.f, 1.f);
						} else {
							mult = cos(fabs(a_this->freeRotation.x));
							desiredPitch = (currentPitch - a_this->freeRotation.y) * mult;
						}

						pitchDelta = desiredPitch - currentPitch;
					}

					bool bMoving = cameraTarget->actorState1.movingBack ||
					               cameraTarget->actorState1.movingForward ||
					               cameraTarget->actorState1.movingRight ||
					               cameraTarget->actorState1.movingLeft;

					if (bMoving || !a_weaponSheathed) {
						//cameraTarget->SetRotationX(cameraTarget->data.angle.x + pitchDelta);
						cameraTarget->data.angle.x += pitchDelta;
						a_this->freeRotation.y += pitchDelta;
					}
				}
			}
		} else {
			_SetFreeRotationMode(a_this, a_weaponSheathed);
		}
	}

	void ThirdPersonStateHook::ProcessButton(RE::ThirdPersonState* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		if (a_event && directionalMovementHandler->HasTargetLocked() && directionalMovementHandler->GetTargetLockUseScrollWheel()) {
			auto userEvent = a_event->QUserEvent();
			auto userEvents = RE::UserEvents::GetSingleton();

			if (userEvent == userEvents->zoomIn) {
				directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kLeft);
				return;
			} else if (userEvent == userEvents->zoomOut) {
				directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kRight);
				return;
			}
		}

		_ProcessButton(a_this, a_event, a_data);
	}
	
	void HorseCameraStateHook::OnEnterState(RE::HorseCameraState* a_this)
	{
		_OnEnterState(a_this);

		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();

			RE::Actor* horse = nullptr;
			horse = static_cast<RE::Actor*>(a_this->horseRefHandle.get().get());

			if (savedCamera.rotationType != SaveCamera::RotationType::kNone) {
				auto rotationType = savedCamera.rotationType;
				RE::NiPoint2 rot = savedCamera.ConsumeXY();
				if (rotationType == SaveCamera::RotationType::kThirdPerson) {
					playerCharacter->data.angle.x = -rot.y;
				}
				
				a_this->freeRotation.x = NormalAbsoluteAngle(rot.x - horse->data.angle.z);
			}

			if (savedCamera.bZoomSaved) {
				a_this->targetZoomOffset = savedCamera.ConsumeZoom();
				a_this->currentZoomOffset = a_this->targetZoomOffset;
				a_this->savedZoomOffset = a_this->targetZoomOffset;
			}

			a_this->horseCurrentDirection = horse->GetHeading(false);
		}
	}

	void HorseCameraStateHook::OnExitState(RE::HorseCameraState* a_this)
	{
		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			savedCamera.SaveXY(a_this->freeRotation, SaveCamera::RotationType::kHorse);
			savedCamera.SaveZoom(a_this->currentZoomOffset);
		}

		_OnExitState(a_this);
	}

	void HorseCameraStateHook::UpdateRotation(RE::HorseCameraState* a_this)
	{
		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			float horseCurrentDirection = a_this->horseCurrentDirection;
			float freeRotationX = a_this->freeRotation.x;

			a_this->freeRotationEnabled = true;

			_UpdateRotation(a_this);

			a_this->horseCurrentDirection = horseCurrentDirection;
			a_this->freeRotation.x = freeRotationX;

			if (a_this->horseRefHandle) {
				RE::Actor* horse = nullptr;
				horse = static_cast<RE::Actor*>(a_this->horseRefHandle.get().get());
				if (horse) {
					float heading = horse->GetHeading(false);

					a_this->freeRotation.x += a_this->horseCurrentDirection - heading;

					sub_140C6E180(a_this->rotation, -a_this->freeRotation.y, 0.0, heading + a_this->freeRotation.x);
					a_this->horseCurrentDirection = heading;
				}
			}
		} else {
			_UpdateRotation(a_this);
		}
	}

	static float* g_fSomething = (float*)REL::ID(509846).address();  // 1DF3658
	static float* g_fSomeKindOfSensitivity = (float*)REL::ID(509884).address();  // 1DF3820
	static float* g_fSome2 = (float*)REL::ID(509862).address();                  // 1DF3718

	void HorseCameraStateHook::HandleLookInput(RE::HorseCameraState* a_this, const RE::NiPoint2& a_input)
	{
		if (DirectionalMovementHandler::GetSingleton()->HasTargetLocked()) {
			return;
		}

		_HandleLookInput(a_this, a_input);
	}

	void HorseCameraStateHook::ProcessButton(RE::HorseCameraState* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		if (a_event && directionalMovementHandler->HasTargetLocked() && directionalMovementHandler->GetTargetLockUseScrollWheel()) {
			auto userEvent = a_event->QUserEvent();
			auto userEvents = RE::UserEvents::GetSingleton();

			if (userEvent == userEvents->zoomIn) {
				directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kLeft);
				return;
			} else if (userEvent == userEvents->zoomOut) {
				directionalMovementHandler->SwitchTarget(DirectionalMovementHandler::Directions::kRight);
				return;
			}
		}

		_ProcessButton(a_this, a_event, a_data);
	}

	void TweenMenuCameraStateHook::OnEnterState(RE::TESCameraState* a_this)
	{
		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			savedCamera.rotationType = SaveCamera::RotationType::kNone;
		}
		
		_OnEnterState(a_this);
	}

	void TweenMenuCameraStateHook::OnExitState(RE::TESCameraState* a_this)
	{
		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			savedCamera.rotationType = SaveCamera::RotationType::kNone;
		}

		_OnExitState(a_this);
	}

	void VATSCameraStateHook::OnExitState(RE::TESCameraState* a_this)
	{
		if (DirectionalMovementHandler::GetSingleton()->GetFreeCameraEnabled()) {
			savedCamera.rotationType = SaveCamera::RotationType::kNone;
		}

		_OnExitState(a_this);
	}

	void PlayerCameraTransitionStateHook::OnEnterState(RE::PlayerCameraTransitionState* a_this)
	{
		if (a_this->transitionFrom->id == RE::CameraStates::kMount && a_this->transitionTo->id == RE::CameraStates::kThirdPerson)
		{
			if (savedCamera.rotationType == SaveCamera::RotationType::kHorse) {
				auto thirdPersonState = static_cast<RE::ThirdPersonState*>(a_this->transitionTo);
				auto playerCharacter = RE::PlayerCharacter::GetSingleton();
				thirdPersonState->freeRotation.x = savedCamera.ConsumeX();
				playerCharacter->data.angle.x = -savedCamera.ConsumeY();
			}
		} else if (a_this->transitionFrom->id == RE::CameraStates::kMount && a_this->transitionTo->id == RE::CameraStates::kFirstPerson) {
			if (savedCamera.rotationType == SaveCamera::RotationType::kHorse) {
				auto playerCharacter = RE::PlayerCharacter::GetSingleton();
				playerCharacter->data.angle.x = -savedCamera.ConsumeY();
			}
		}

		_OnEnterState(a_this);
	}

	void MovementHandlerAgentPlayerControlsHook::Func1(void* a1, void* a2)
	{
		// disable dampening controls while sprinting during lock-on, so you don't retain any weird momentum when rotating back to target after sprinting
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (DirectionalMovementHandler::GetSingleton()->HasTargetLocked() && playerCharacter && playerCharacter->IsSprinting()) {
			*g_bDampenPlayerControls = false;
		} else {
			*g_bDampenPlayerControls = true;
		}
		_Func1(a1, a2);
	}

	void ProjectileHook::GetLinearVelocity(RE::Projectile* a_this, RE::NiPoint3& a_outVelocity)
	{
		auto projectileNode = a_this->Get3D2();
		
		// player only, 0x100000 == player
		if (projectileNode && a_this->shooter.native_handle() == 0x100000)
		{
			auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
			if (directionalMovementHandler->HasTargetLocked() || a_this->desiredTarget.native_handle() != 0)
			{
				DirectionalMovementHandler::TargetLockProjectileAimType aimType;

				switch (a_this->formType.get())
				{
				case RE::FormType::ProjectileArrow:
					aimType = directionalMovementHandler->GetTargetLockArrowAimType();
					break;
				case RE::FormType::ProjectileMissile:
					aimType = directionalMovementHandler->GetTargetLockMissileAimType();
					break;
				default:
					aimType = DirectionalMovementHandler::kFreeAim;
				}

				if (aimType != DirectionalMovementHandler::kFreeAim)
				{
					if (!a_this->desiredTarget.get()) {
						auto target = directionalMovementHandler->GetTarget();
						if (!target)
						{
							return _GetLinearVelocity(a_this, a_outVelocity);
						}
						a_this->desiredTarget = target;

						if (aimType == DirectionalMovementHandler::kPredict)
						{
							// predict only at the start (desiredTarget not yet set), then let the projectile go unchanged in next updates
							RE::NiPoint3 targetPos;
							if (GetTargetPos(a_this->desiredTarget, targetPos))
							{
								RE::NiPoint3 targetVelocity;
								target.get()->GetLinearVelocity(targetVelocity);

								float projectileGravity = 0.f;
								auto ammo = a_this->ammoSource;
								if (ammo)
								{
									auto bgsProjectile = ammo->data.projectile;
									if (bgsProjectile)
									{
										projectileGravity = bgsProjectile->data.gravity;
									}
								}

								PredictAimProjectile(a_this->data.location, targetPos, targetVelocity, projectileGravity, a_this->linearVelocity);
								
								// rotate
								RE::NiPoint3 direction = a_this->linearVelocity;
								direction.Unitize();

								a_this->data.angle.x = asin(direction.z);
								a_this->data.angle.z = atan2(direction.x, direction.y);

								if (a_this->data.angle.z < 0.0) {
									a_this->data.angle.z += PI;
								}

								if (direction.x < 0.0) {
									a_this->data.angle.z += PI;
								}

								SetRotationMatrix(projectileNode->local.rotate, -direction.x, direction.y, direction.z);
							}
						}
					}

					RE::NiPoint3 targetPos;
					if (aimType == DirectionalMovementHandler::kHoming && GetTargetPos(a_this->desiredTarget, targetPos)) 
					{
						// homing
						RE::NiPoint3& velocity = a_this->linearVelocity;
						float speed = velocity.Length();

						//if (speed < 1500.f) {
						//	return _GetLinearVelocity(a_this, a_outVelocity);
						//}

						RE::NiPoint3 direction = (targetPos - a_this->data.location);

						// normalize direction
						direction.Unitize();

						// rotate
						a_this->data.angle.x = asin(direction.z);
						a_this->data.angle.z = atan2(direction.x, direction.y);

						if (a_this->data.angle.z < 0.0) {
							a_this->data.angle.z += PI;
						}

						if (direction.x < 0.0) {
							a_this->data.angle.z += PI;
						}

						SetRotationMatrix(projectileNode->local.rotate, -direction.x, direction.y, direction.z);
						velocity = direction * speed;
					}
				}
			}
		}

		_GetLinearVelocity(a_this, a_outVelocity);
	}

	void ProjectileHook::Func183(RE::Projectile* a_this)
	{
		_Func183(a_this);

		// player only, 0x100000 == player
		if (a_this->shooter.native_handle() == 0x100000) {
			auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
			if (directionalMovementHandler->HasTargetLocked() || a_this->desiredTarget.native_handle() != 0) {
				auto beamProjectile = skyrim_cast<RE::BeamProjectile*>(a_this);
				auto target = directionalMovementHandler->GetTarget();
				if (!beamProjectile || !target) {
					return;
				}
				a_this->desiredTarget = target;

				RE::NiPoint3 targetPos;

				if (GetTargetPos(a_this->desiredTarget, targetPos))
				{
					RE::NiPoint3 direction = (targetPos - a_this->data.location);

					// normalize direction
					direction.Unitize();

					// rotate
					a_this->data.angle.x = atan2(-direction.z, std::sqrtf(direction.x * direction.x + direction.y * direction.y));
					a_this->data.angle.z = atan2(direction.x, direction.y);

					if (a_this->data.angle.z < 0.0) {
						a_this->data.angle.z += PI;
					}

					if (direction.x < 0.0) {
						a_this->data.angle.z += PI;
					}
				}
			}
		}
	}

	void PlayerCharacterHook::ProcessTracking(RE::Actor* a_this, float a_delta, RE::NiAVObject* a_obj3D)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();

		bool bBehaviorPatchInstalled = DirectionalMovementHandler::IsBehaviorPatchInstalled(a_this);

		if (bBehaviorPatchInstalled)
		{
			// for good measure
			a_this->SetGraphVariableBool("IsNPC", false);
		}

		if (directionalMovementHandler->IsIFPV() || directionalMovementHandler->IsImprovedCamera())
		{
			a_this->actorState2.headTracking = false;
			a_this->SetGraphVariableBool("IsNPC", false);
			return _ProcessTracking(a_this, a_delta, a_obj3D);
		}
				
		bool bIsHeadtrackingEnabled = directionalMovementHandler->IsHeadtrackingEnabled();
		bool bIsBlocking = false;
		bool bIsSprinting = false;

		if (bIsHeadtrackingEnabled && a_this->currentProcess) {
			if (!bBehaviorPatchInstalled) {
				a_this->actorState2.headTracking = true;
				a_this->SetGraphVariableBool("IsNPC", true);
			}
			
			// expire dialogue headtrack if timer is up
			if (a_this->currentProcess->high && a_this->currentProcess->high->headTrack3) {
				if (directionalMovementHandler->GetDialogueHeadtrackTimer() <= 0.f) {
					a_this->currentProcess->high->SetHeadtrackTarget(3, nullptr);
				}
			}

			// set headtracking variables if we have any target set
			auto target = a_this->currentProcess->GetHeadtrackTarget();
			auto cameraState = RE::PlayerCamera::GetSingleton()->currentState;
			if (target) {
				a_this->actorState2.headTracking = true;
				if (!bBehaviorPatchInstalled) {
					a_this->SetGraphVariableBool("IsNPC", true);
				}
			} else {
				a_this->actorState2.headTracking = false;
				/*if (!bBehaviorPatchInstalled) {
					a_this->SetGraphVariableBool("IsNPC", false);
				}	*/
			}

			bIsBlocking = a_this->actorState2.wantBlocking;
			bIsSprinting = a_this->actorState1.sprinting;
			// disable headtracking while attacking or blocking without behavior patch
			if ((!bBehaviorPatchInstalled && bIsBlocking) ||
				(a_this->actorState1.meleeAttackState > RE::ATTACK_STATE_ENUM::kNone)) {
				a_this->actorState2.headTracking = false;
				if (!bBehaviorPatchInstalled) {
					a_this->SetGraphVariableBool("IsNPC", false);
				}
			}
		}

		RE::NiPoint3 targetPos;

		// reset headtrack
		if (a_this->currentProcess && a_this->currentProcess->high) {
			// clear the 0 and 1 targets if they're set for whatever reason
			auto selfHandle = a_this->GetHandle();
			if (a_this->currentProcess->high->headTrack0) {
				a_this->currentProcess->high->SetHeadtrackTarget(0, nullptr);
			}
			if (a_this->currentProcess->high->headTrack1) {
				a_this->currentProcess->high->SetHeadtrackTarget(1, nullptr);
			}

			targetPos = a_this->GetLookingAtLocation();
			float yaw = NormalRelativeAngle(a_this->data.angle.z - PI2);
			RE::NiPoint3 offset = -RotationToDirection(yaw, 0) * 500.f;
			offset.x *= -1.f;
			targetPos += offset;
			a_this->currentProcess->high->headTrackTargetOffset = targetPos;
			a_this->currentProcess->SetHeadtrackTarget(a_this, targetPos);
		}

		// run original function
		_ProcessTracking(a_this, a_delta, a_obj3D);

		// handle fake IsNPC
		if (!bIsHeadtrackingEnabled && bBehaviorPatchInstalled && !DirectionalMovementHandler::GetSingleton()->GetPlayerIsNPC()) {
			a_this->currentProcess->SetHeadtrackTarget(a_this, targetPos);
		}

		if (bIsHeadtrackingEnabled && !bIsSprinting && !bIsBlocking && directionalMovementHandler->IsCameraHeadtrackingEnabled() && a_this->currentProcess && a_this->boolBits.none(RE::Actor::BOOL_BITS::kHasSceneExtra)) {
			// try camera headtracking
			auto highProcess = a_this->currentProcess->high;
			if (highProcess && 
				a_this->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kNone &&
				a_this->actorState1.sitSleepState != RE::SIT_SLEEP_STATE::kIsSleeping &&
				!highProcess->headTrack2 && !highProcess->headTrack3 && !highProcess->headTrack4 && !highProcess->headTrack5)
			{
				if (!bBehaviorPatchInstalled) {
					a_this->SetGraphVariableBool("IsNPC", true);
				}

				directionalMovementHandler->UpdateCameraHeadtracking();
			}
		}
	}

	/*std::string time_in_HH_MM_SS_MMM()
	{
		using namespace std::chrono;

		// get current time
		auto now = system_clock::now();

		// get number of milliseconds for the current second
		// (remainder after division into seconds)
		auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

		// convert to std::time_t in order to convert to std::tm (broken time)
		auto timer = system_clock::to_time_t(now);

		// convert to broken time
		std::tm bt;
		localtime_s(&bt, &timer);

		std::ostringstream oss;

		oss << std::put_time(&bt, "%H:%M:%S");	// HH:MM:SS
		oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

		return oss.str();
	}*/

	constexpr uint32_t hash(const char* data, size_t const size) noexcept
	{
		uint32_t hash = 5381;

		for (const char* c = data; c < data + size; ++c) {
			hash = ((hash << 5) + hash) + (unsigned char)*c;
		}

		return hash;
	}

	constexpr uint32_t operator"" _h(const char* str, size_t size) noexcept
	{
		return hash(str, size);
	}

	void PlayerCharacterHook::ProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_dispatcher)
	{
		if (a_event)
		{
			std::string_view eventTag = a_event->tag.data();

			switch (hash(eventTag.data(), eventTag.size())) {
			// Start phase
			case "CastOKStart"_h:
			case "preHitFrame"_h:
			case "TDM_AttackStart"_h:
				DirectionalMovementHandler::GetSingleton()->SetAttackState(DirectionalMovementHandler::AttackState::kStart);
				break;

			// Mid phase. Ignore vanilla events if we're tracing already
			case "weaponSwing"_h:
			case "weaponLeftSwing"_h:
				if (DirectionalMovementHandler::GetSingleton()->GetAttackState() != DirectionalMovementHandler::kTracing)
				{
					DirectionalMovementHandler::GetSingleton()->SetAttackState(DirectionalMovementHandler::AttackState::kMid);
				}
				break;
			case "TDM_AttackMid"_h:
			case "MeleeTrace_Right_Start"_h:
			case "MeleeTrace_Left_Start"_h:
				DirectionalMovementHandler::GetSingleton()->SetAttackState(DirectionalMovementHandler::AttackState::kMid);
				break;

			// End phase. Ignore vanilla events if we're tracing already
			case "HitFrame"_h:
				if (DirectionalMovementHandler::GetSingleton()->GetAttackState() != DirectionalMovementHandler::kTracing)
				{
					DirectionalMovementHandler::GetSingleton()->SetAttackState(DirectionalMovementHandler::AttackState::kEnd);
				}
				break;
			case "TDM_AttackEnd"_h:
			case "MeleeTrace_Right_Stop"_h:
			case "MeleeTrace_Left_Stop"_h:
				DirectionalMovementHandler::GetSingleton()->SetAttackState(DirectionalMovementHandler::AttackState::kEnd);
				break;

			// Back to none
			case "attackStop"_h:
			case "TDM_AttackStop"_h:
				DirectionalMovementHandler::GetSingleton()->SetAttackState(DirectionalMovementHandler::AttackState::kNone);
				break;
			}
			
			//logger::info("{} - {}", time_in_HH_MM_SS_MMM(), a_event->tag);
		}
		_ProcessEvent(a_this, a_event, a_dispatcher);
	}

	static void ApplyYawDelta(RE::ActorState* a_actorState, RE::NiPoint3& a_angle)
	{
		auto actor = static_cast<RE::Actor*>(a_actorState);
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		bool bIsAIDriven = actor->movementController && !actor->movementController->unk1C5;
		if (!bIsAIDriven) {
			a_angle.z -= DirectionalMovementHandler::GetSingleton()->GetYawDelta();
		} else {
			directionalMovementHandler->ResetYawDelta();
		}
	}

	void PlayerCharacterHook::GetAngle(RE::ActorState* a_this, RE::NiPoint3& a_angle)
	{
		_GetAngle(a_this, a_angle);
		
		ApplyYawDelta(a_this, a_angle);
	}

	void PlayerCharacterHook::Sprint(RE::PlayerCharacter* a_this)
	{
		auto playerControls = RE::PlayerControls::GetSingleton();
		RE::NiPointer<RE::Actor> mount = nullptr;
		bool bMounted = a_this->GetMount(mount);
		auto actor = bMounted ? mount.get() : a_this;
		if (a_this != nullptr) {
			bool bShouldBeSprinting = false;

			bool bIsSyncSprintState = Actor__IsSyncSprintState_140608800(actor);
			bool bIsSprintingRunningOrBlocking = actor->actorState1.sprinting == true || actor->IsRunning() || actor->IsBlocking();
			bool bUnk1 = Actor__sub_1405D16B0(actor);
			bool bIsOverEncumbered = actor->IsOverEncumbered();
			bool bUnk2 = Actor__IsSyncSprintState_140608800(a_this) || (actor->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone);
			bool bIsPreviousMoveInputForward = playerControls->data.prevMoveVec.y > 0.f;
			bool bIsNotStrafing = *g_fSprintStopThreshold > fabs(playerControls->data.prevMoveVec.x);
			bool bIsStaminaNotZero = actor->GetActorValue(RE::ActorValue::kStamina) > 0.f;
			bool bHasUnkBDD_SprintingFlag = (a_this->unkBDD & RE::PlayerCharacter::FlagBDD::kSprinting) != RE::PlayerCharacter::FlagBDD::kNone;

			// added
			auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
			bool bHasMovementInput = playerControls->data.prevMoveVec.x != 0.f || playerControls->data.prevMoveVec.y != 0.f;
			bool bIsAttacking = directionalMovementHandler->GetAttackState() != DirectionalMovementHandler::AttackState::kNone;
			bool bIsDodging = directionalMovementHandler->IsDodging();
			int iState;
			a_this->GetGraphVariableInt("iState", iState);
			bool bIsCasting = iState == 10;
			bool bFreeCamTargetLocked = directionalMovementHandler->HasTargetLocked() && directionalMovementHandler->IsFreeCamera();
			
			if (bMounted)
			{
				bIsPreviousMoveInputForward = bHasMovementInput;
				bIsNotStrafing = bHasMovementInput;
			}

			bool bSpecific = bFreeCamTargetLocked ? bHasMovementInput && !bIsAttacking && !bIsDodging && !bIsCasting : bIsSprintingRunningOrBlocking && bIsPreviousMoveInputForward && bIsNotStrafing; // branch depending on the mode we're in

			if (bHasUnkBDD_SprintingFlag &&
				!bUnk1 &&
				!bIsOverEncumbered &&
				bUnk2 &&
				bIsStaminaNotZero &&
				bSpecific)
			{
				bShouldBeSprinting = true;
			} else {
				bShouldBeSprinting = false;
				a_this->unkBDD.reset(RE::PlayerCharacter::FlagBDD::kSprinting);
			}

			if (bIsSyncSprintState != bShouldBeSprinting) {
				PlayerControls__sub_140705530(playerControls, 66 - bShouldBeSprinting, 2);	// ?
			}
		}
	}

	void AIProcess_SetRotationSpeedZHook::AIProcess_SetRotationSpeedZ(RE::AIProcess* a_this, float a_rotationSpeed)
	{
		if (a_this) {
			if (RE::PlayerCharacter::GetSingleton()->currentProcess == a_this && DirectionalMovementHandler::GetSingleton()->IsFreeCamera()) {
				return;	 // skip because we're setting it elsewhere and it'd overwrite to 0
			}
			a_this->middleHigh->rotationSpeed.z = a_rotationSpeed;
		}
	}

	void Actor_SetRotationHook::Actor_SetRotationX(RE::Actor* a_this, float a_angle)
	{
		if (a_this->IsPlayerRef()) {
			auto thirdPersonState = static_cast<RE::ThirdPersonState*>(RE::PlayerCamera::GetSingleton()->cameraStates[RE::CameraState::kThirdPerson].get());
			if (RE::PlayerCamera::GetSingleton()->currentState.get() == thirdPersonState && thirdPersonState->freeRotationEnabled) {
				float angleDelta = a_angle - a_this->data.angle.x;
				thirdPersonState->freeRotation.y += angleDelta;
			}
		}

		_Actor_SetRotationX(a_this, a_angle);
	}

	void Actor_SetRotationHook::Actor_SetRotationZ(RE::Actor* a_this, float a_angle)
	{
		if (a_this->IsPlayerRef())
		{
			auto thirdPersonState = static_cast<RE::ThirdPersonState*>(RE::PlayerCamera::GetSingleton()->cameraStates[RE::CameraState::kThirdPerson].get());
			if (RE::PlayerCamera::GetSingleton()->currentState.get() == thirdPersonState && thirdPersonState->freeRotationEnabled) {
				float angleDelta = a_angle - a_this->data.angle.z;
				thirdPersonState->freeRotation.x -= angleDelta;
			}
		}

		_Actor_SetRotationZ(a_this, a_angle);
	}

	bool EnemyHealthHook::ProcessMessage(uintptr_t a_enemyHealth, RE::HUDData* a_hudData)
	{
		bool bReturn = _ProcessMessage(a_enemyHealth, a_hudData);

		RE::RefHandle* refHandle = (RE::RefHandle*)(a_enemyHealth + 0x28);

		WidgetHandler::GetSingleton()->SetEnemyHealthTargetRef(*refHandle);

		if (WidgetHandler::ShowSoftTargetBar()) {
			if (*refHandle) {
				auto actorPtr = RE::Actor::LookupByHandle(*refHandle);
				if (actorPtr) {
					DirectionalMovementHandler::GetSingleton()->SetSoftTarget(actorPtr->GetHandle());
				}
			} else {
				DirectionalMovementHandler::GetSingleton()->SetSoftTarget(RE::ActorHandle());
			}
		}

		return bReturn;
	}

	void HeadtrackingHook::SetHeadtrackTarget0(RE::AIProcess* a_this, RE::Actor* a_target)
	{	
		// Skip for player so we don't get random headtracking targets
		if (DirectionalMovementHandler::GetSingleton()->IsHeadtrackingEnabled() && a_this == RE::PlayerCharacter::GetSingleton()->currentProcess) {
			_SetHeadtrackTarget0(a_this, nullptr);
			return;
		}
		_SetHeadtrackTarget0(a_this, a_target);
	}

	// ridiculous, I know
	RE::TESObjectREFR* RecursiveSearchForParent(RE::NiAVObject* a_object)
	{
		if (!a_object) {
			return nullptr;
		}

		if (a_object->userData) {
			return a_object->userData;
		}
		else if (a_object->parent) {
			return RecursiveSearchForParent(a_object->parent);
		}
		return nullptr;
	}

	void HeadtrackingHook::SetHeadtrackTarget4(RE::AIProcess* a_this, RE::Actor* a_target)
	{
		_SetHeadtrackTarget4(a_this, a_target);

		if (DirectionalMovementHandler::GetSingleton()->IsHeadtrackingEnabled() && a_target && a_target->IsPlayerRef() && a_this->middleHigh)
		{
			RE::NiAVObject* object = a_this->middleHigh->torsoNode;
			if (!object) {
				object = a_this->middleHigh->headNode;
			}
			auto refr = RecursiveSearchForParent(object);
			if (refr) {
				auto actor = refr->As<RE::Actor>();
				if (actor) {
					//_SetHeadtrackTarget4(a_target->currentProcess, actor);
					if (a_target->currentProcess && a_target->currentProcess->high) {
						a_target->currentProcess->high->SetHeadtrackTarget(3, actor);  // for player, use lower priority so target lock overrides dialogue targets
						DirectionalMovementHandler::GetSingleton()->RefreshDialogueHeadtrackTimer();
					}
				}
			}
		}
	}

	bool PlayerControlsHook::Handle(RE::PlayerControls* a_this, uintptr_t a2)
	{
		DirectionalMovementHandler::ResetControls();
		return _Handle(a_this, a2);
	}

	bool PlayerControlsHook::CanProcessControls(RE::PlayerControls* a_this, RE::InputEvent** a_eventPtr)
	{
		bool bCanProcessControls = _CanProcessControls(a_this, a_eventPtr);

		// process camera movement during locked controls
		if (!bCanProcessControls && !RE::UI::GetSingleton()->GameIsPaused()) {
			for (RE::InputEvent* inputEvent = *a_eventPtr; inputEvent != nullptr; inputEvent = inputEvent->next) {
				if (inputEvent->eventType == RE::INPUT_EVENT_TYPE::kMouseMove) {
					RE::MouseMoveEvent* mouseMoveEvent = static_cast<RE::MouseMoveEvent*>(inputEvent);
					a_this->lookHandler->ProcessMouseMove(mouseMoveEvent, &a_this->data);
					PlayerControls__ApplyLookSensitivitySettings_140705AE0(a_this, &a_this->data.lookInputVec);
				} else if (inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
					RE::ThumbstickEvent* thumbstickEvent = static_cast<RE::ThumbstickEvent*>(inputEvent);
					if (thumbstickEvent->IsRight()) {
						a_this->lookHandler->ProcessThumbstick(thumbstickEvent, &a_this->data);
						PlayerControls__ApplyLookSensitivitySettings_140705AE0(a_this, &a_this->data.lookInputVec);
					}
				}
			}
		}

		return bCanProcessControls;
	}

	void NukeSetIsNPCHook::SetBool(RE::IAnimationGraphManagerHolder* a_this, RE::BSFixedString* a_variableName, bool a_value)
	{
		if (a_variableName && a_variableName->c_str() == "IsNPC"sv)
		{
			auto ref = static_cast<RE::TESObjectREFR*>(a_this);
			auto formID = ref->formID;
			if (formID == 0x14 && DirectionalMovementHandler::IsBehaviorPatchInstalled(ref)) // player
			{
				DirectionalMovementHandler::GetSingleton()->SetPlayerIsNPC(a_value);
				a_value = false;
			}
		}
		_SetBool(a_this, a_variableName, a_value);
	}

	void NukeSetIsNPCHook::SetInt(RE::IAnimationGraphManagerHolder* a_this, RE::BSFixedString* a_variableName, int32_t a_value)
	{
		if (a_variableName && a_variableName->c_str() == "IsNPC"sv) {
			auto ref = static_cast<RE::TESObjectREFR*>(a_this);
			auto formID = ref->formID;
			if (formID == 0x14 && DirectionalMovementHandler::IsBehaviorPatchInstalled(ref))  // player
			{
				DirectionalMovementHandler::GetSingleton()->SetPlayerIsNPC(a_value);
				a_value = 0;
			}
		}
		_SetInt(a_this, a_variableName, a_value);
	}

	void PlayerCameraHook::Update(RE::TESCamera* a_this)
	{
		_Update(a_this);

		DirectionalMovementHandler::GetSingleton()->UpdatePlayerPitch();
	}

	void MainUpdateHook::Update(RE::Main* a_this, float a2)
	{
		_Update(a_this, a2);

		DirectionalMovementHandler::GetSingleton()->Update();
	}

	static float angleToTarget = 0.f;
	float* HorseAimHook::GetHorseCameraFreeRotationYaw(RE::PlayerCamera* a_this)
	{
		auto directionalMovementHandler = DirectionalMovementHandler::GetSingleton();
		if (directionalMovementHandler->HasTargetLocked() && directionalMovementHandler->GetTarget())
		{
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			auto target = directionalMovementHandler->GetTarget();
			RE::NiPoint2 playerPos;
			playerPos.x = playerCharacter->GetPositionX();
			playerPos.y = playerCharacter->GetPositionY();
			RE::NiPoint2 targetPos;
			targetPos.x = target.get()->GetPositionX();
			targetPos.y = target.get()->GetPositionY();

			RE::NiPoint2 directionToTarget = RE::NiPoint2(-(targetPos.x - playerPos.x), targetPos.y - playerPos.y);
			directionToTarget.Unitize();

			RE::NiPoint2 forwardVector(0.f, 1.f);
			RE::NiPoint2 currentCharacterDirection = Vec2Rotate(forwardVector, playerCharacter->data.angle.z);

			angleToTarget = GetAngle(currentCharacterDirection, directionToTarget);

			return &angleToTarget;
		}
		return _GetHorseCameraFreeRotationYaw(a_this);
	}

	void HorseAimHook::Func(RE::PlayerCamera* a_this)
	{	
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (DirectionalMovementHandler::GetSingleton()->HasTargetLocked() && playerCharacter && playerCharacter->IsOnMount()) {
			return;
		}

		_Func(a_this);
	}
}
