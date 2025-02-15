use jsapi::root::*;
#[cfg(feature = "debugmozjs")]
use std::ptr;

#[derive(Debug)]
pub struct AutoCompartment(JSAutoRealmAllowCCW);

impl AutoCompartment {
    #[cfg(feature = "debugmozjs")]
    pub unsafe fn with_obj(cx: *mut JSContext,
                           target: *mut JSObject)
                           -> AutoCompartment
    {
        let mut notifier = mozilla::detail::GuardObjectNotifier {
            mStatementDone: ptr::null_mut(),
        };

        AutoCompartment(
            JSAutoRealmAllowCCW::new(
                cx,
                target,
                &mut notifier as *mut _))
    }

    #[cfg(not(feature = "debugmozjs"))]
    pub unsafe fn with_obj(cx: *mut JSContext,
                           target: *mut JSObject)
                           -> AutoCompartment
    {
        AutoCompartment(JSAutoRealmAllowCCW::new(cx, target))
    }

    #[cfg(feature = "debugmozjs")]
    pub unsafe fn with_script(cx: *mut JSContext,
                              target: *mut JSScript)
                              -> AutoCompartment
    {
        let mut notifier = mozilla::detail::GuardObjectNotifier {
            mStatementDone: ptr::null_mut(),
        };

        AutoCompartment(
            JSAutoRealmAllowCCW::new1(
                cx,
                target,
                &mut notifier as *mut _))
    }

    #[cfg(not(feature = "debugmozjs"))]
    pub unsafe fn with_script(cx: *mut JSContext,
                              target: *mut JSScript)
                              -> AutoCompartment
    {
        AutoCompartment(JSAutoRealmAllowCCW::new1(cx, target))
    }
}
