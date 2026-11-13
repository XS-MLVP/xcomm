// Container copies share immutable type metadata and cached child nodes. They
// are independent handles, not copies of signal data or DUT backends.
%define %owned_view(Class, Method, Result, Declaration, Argument)
%rename(Method) xspcomm::Class::Method##Handle;
%newobject xspcomm::Class::Method##Handle;
%extend xspcomm::Class {
    xspcomm::Result *Method##Handle(Declaration) {
        return new xspcomm::Result(*$self->Method(Argument));
    }
}
%enddef

%define %owned_node(Class, Method, Declaration, Argument)
%rename(Method) xspcomm::Class::Method##Handle;
%newobject xspcomm::Class::Method##Handle;
%extend xspcomm::Class {
    xspcomm::XView *Method##Handle(Declaration) {
        return new xspcomm::XView($self->Method(Argument));
    }
}
%enddef

%define %borrowed_data(Class, Method, Declaration, Argument)
%rename(Method) xspcomm::Class::Method##Handle;
%extend xspcomm::Class {
    xspcomm::XData *Method##Handle(Declaration) {
        return $self->Method(Argument).get();
    }
}
%enddef

%owned_view(XData, StructView, XStruct, const std::string &layout, layout)
%owned_view(XData, ArrayView, XArray, const std::string &layout, layout)
%owned_view(XView, Struct, XStruct, , )
%owned_view(XView, Array, XArray, , )
%owned_node(XStruct, Field, const std::string &name, name)
%owned_view(XStruct, Struct, XStruct, const std::string &name, name)
%owned_view(XStruct, Array, XArray, const std::string &name, name)
%owned_node(XArray, At, int index, index)
%owned_view(XArray, Struct, XStruct, int index, index)
%owned_view(XArray, Array, XArray, int index, index)
%owned_node(XVPI, Get, const std::string &name, name)
%borrowed_data(XView, Data, , )
%borrowed_data(XView, PackedData, , )
%borrowed_data(XStruct, Data, const std::string &name, name)
%borrowed_data(XStruct, PackedData, , )
%borrowed_data(XArray, Data, int index, index)
%borrowed_data(XArray, PackedData, , )

%rename(FromData) xspcomm::XView::FromDataHandle;
%rename(FromPort) xspcomm::XView::FromPortHandle;
%newobject xspcomm::XView::FromDataHandle;
%newobject xspcomm::XView::FromPortHandle;
%extend xspcomm::XView {
    static xspcomm::XView *FromDataHandle(xspcomm::XData &data, const std::string &layout) {
        return new xspcomm::XView(xspcomm::XView::FromData(data, layout));
    }
    static xspcomm::XView *FromPortHandle(xspcomm::XPort &port, const std::string &name,
                                       const std::string &layout) {
        return new xspcomm::XView(xspcomm::XView::FromPort(port, name, layout));
    }
}
