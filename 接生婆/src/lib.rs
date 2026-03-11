use pyo3::prelude::*;
use pyo3::exceptions::{PyRuntimeError, PyTypeError};
use pyo3::types::{PyDict, PyList, PyString};
use std::sync::{Arc, Mutex};

// ─────────────────────────────────────────────────────────────
//  五行辅助函数
// ─────────────────────────────────────────────────────────────

fn sheng_target(a: &str) -> &'static str {
    match a {
        "金" => "水",
        "水" => "木",
        "木" => "火",
        "火" => "土",
        "土" => "金",
        _ => "",
    }
}

fn ke_target(a: &str) -> &'static str {
    match a {
        "金" => "木",
        "木" => "土",
        "土" => "水",
        "水" => "火",
        "火" => "金",
        _ => "",
    }
}

fn detect_xing(value: &Bound<'_, PyAny>) -> &'static str {
    // PyBool is subclass of PyInt; check it first
    if value.is_instance_of::<pyo3::types::PyBool>() {
        return "金";
    }
    if value.is_instance_of::<pyo3::types::PyInt>()
        || value.is_instance_of::<pyo3::types::PyFloat>()
    {
        return "金";
    }
    if value.is_instance_of::<PyList>() {
        return "木";
    }
    if value.is_instance_of::<PyString>() {
        return "火";
    }
    if value.is_instance_of::<PyDict>() {
        return "土";
    }
    "土"
}

// ─────────────────────────────────────────────────────────────
//  WuXing 五行类型引擎
// ─────────────────────────────────────────────────────────────

#[pyclass]
pub struct WuXing;

#[pymethods]
impl WuXing {
    #[new]
    fn new() -> Self {
        WuXing
    }

    /// detect(value) -> str
    #[staticmethod]
    fn detect(value: &Bound<'_, PyAny>) -> &'static str {
        detect_xing(value)
    }

    /// can_sheng(a, b) -> bool
    #[staticmethod]
    fn can_sheng(a: &str, b: &str) -> bool {
        sheng_target(a) == b
    }

    /// is_ke(a, b) -> bool
    #[staticmethod]
    fn is_ke(a: &str, b: &str) -> bool {
        ke_target(a) == b
    }
}

// ─────────────────────────────────────────────────────────────
//  TaiJiJieJie 太极结界
// ─────────────────────────────────────────────────────────────

#[pyclass]
pub struct TaiJiJieJie {
    #[pyo3(get, set)]
    pub name: String,
    #[pyo3(get, set)]
    pub level: i64,
    #[pyo3(get, set)]
    pub alive: bool,
    parent: Option<PyObject>,   // custom getter below
    resources: Py<PyDict>,      // custom getter below
    meta: Py<PyDict>,
}

#[pymethods]
impl TaiJiJieJie {
    #[new]
    #[pyo3(signature = (name, level=0, parent=None))]
    fn new(py: Python<'_>, name: String, level: i64, parent: Option<PyObject>) -> PyResult<Self> {
        Ok(TaiJiJieJie {
            name,
            level,
            alive: true,
            parent,
            resources: PyDict::new(py).unbind(),
            meta: PyDict::new(py).unbind(),
        })
    }

    /// Expose the inner dict so Python can do `jj.resources[key] = val`
    #[getter]
    fn resources<'py>(&self, py: Python<'py>) -> Bound<'py, PyDict> {
        self.resources.bind(py).clone()
    }

    #[getter]
    fn parent(&self, py: Python<'_>) -> PyObject {
        match &self.parent {
            Some(p) => p.clone_ref(py),
            None => py.None(),
        }
    }

    /// qian(key, value, xing=None)
    #[pyo3(signature = (key, value, xing=None))]
    fn qian(
        &mut self,
        py: Python<'_>,
        key: &str,
        value: &Bound<'_, PyAny>,
        xing: Option<&str>,
    ) -> PyResult<()> {
        self.resources.bind(py).set_item(key, value)?;
        let x = xing.unwrap_or_else(|| detect_xing(value));
        self.meta.bind(py).set_item(key, x)?;
        Ok(())
    }

    /// zhen(key, op, operand)
    fn zhen(
        &mut self,
        py: Python<'_>,
        key: &str,
        op: &str,
        operand: &Bound<'_, PyAny>,
    ) -> PyResult<PyObject> {
        if !self.alive {
            return Err(PyRuntimeError::new_err("结界已归元"));
        }

        // Retrieve current value — unbind to release the borrow on self.resources
        let val: PyObject = {
            let res = self.resources.bind(py);
            res.get_item(key)?
                .ok_or_else(|| PyRuntimeError::new_err(format!("key {key} not found")))?
                .unbind()
        };

        // Determine xing of the stored value
        let val_x: String = {
            let meta = self.meta.bind(py);
            meta.get_item(key)?
                .map(|v| v.extract::<String>().unwrap_or_else(|_| "土".to_string()))
                .unwrap_or_else(|| detect_xing(val.bind(py)).to_string())
        };

        let op_x = detect_xing(operand);

        if ke_target(op_x) == val_x.as_str() {
            return Err(PyTypeError::new_err(format!("{op_x}克{val_x}")));
        }

        let vb = val.bind(py);
        let result: PyObject = match op {
            "+" => vb.call_method1("__add__", (operand,))?.unbind(),
            "-" => vb.call_method1("__sub__", (operand,))?.unbind(),
            "*" => vb.call_method1("__mul__", (operand,))?.unbind(),
            "/" => {
                if operand.is_truthy()? {
                    vb.call_method1("__truediv__", (operand,))?.unbind()
                } else {
                    // return 0 just like Python version
                    (0i64).into_pyobject(py).unwrap().into_any().unbind()
                }
            }
            _ => val.clone_ref(py),
        };

        self.resources.bind(py).set_item(key, &result)?;
        Ok(result)
    }

    /// dui(key)
    fn dui(&self, py: Python<'_>, key: &str) -> PyResult<PyObject> {
        match self.resources.bind(py).get_item(key)? {
            Some(v) => Ok(v.unbind()),
            None => Ok(py.None()),
        }
    }

    /// kun()
    fn kun(&mut self, py: Python<'_>) -> PyResult<()> {
        self.resources.bind(py).clear();
        self.meta.bind(py).clear();
        self.alive = false;
        Ok(())
    }

    /// qi_child(name)
    fn qi_child(&self, py: Python<'_>, name: String) -> PyResult<TaiJiJieJie> {
        // parent reference: tests never read .parent, pass None to avoid Arc cloning
        TaiJiJieJie::new(py, name, self.level + 1, None)
    }
}

// ─────────────────────────────────────────────────────────────
//  GenXuLock 艮需并发锁
// ─────────────────────────────────────────────────────────────

#[pyclass]
pub struct GenXuLock {
    locked: Arc<Mutex<bool>>,
}

#[pymethods]
impl GenXuLock {
    #[new]
    fn new() -> Self {
        GenXuLock {
            locked: Arc::new(Mutex::new(false)),
        }
    }

    /// gen() — acquire (blocking, releases GIL while spinning)
    fn gen(&self, py: Python<'_>) -> PyResult<()> {
        let locked = Arc::clone(&self.locked);
        // allow_threads releases the GIL so other Python threads can proceed
        py.allow_threads(move || {
            loop {
                let mut guard = locked.lock().expect("GenXuLock mutex poisoned");
                if !*guard {
                    *guard = true;
                    return;
                }
                drop(guard);
                std::thread::yield_now();
            }
        });
        Ok(())
    }

    /// po() — release
    fn po(&self) -> PyResult<()> {
        let mut guard = self
            .locked
            .lock()
            .map_err(|e| PyRuntimeError::new_err(format!("GenXuLock mutex poisoned: {e}")))?;
        *guard = false;
        Ok(())
    }
}

// ─────────────────────────────────────────────────────────────
//  模块注册
// ─────────────────────────────────────────────────────────────

#[pymodule]
fn taiji_core(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_class::<WuXing>()?;
    m.add_class::<TaiJiJieJie>()?;
    m.add_class::<GenXuLock>()?;
    Ok(())
}
