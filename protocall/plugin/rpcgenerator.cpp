/******************************************************************************

 This source file is part of the ProtoCall project.

 Copyright 2013 Kitware, Inc.

 This source code is released under the New BSD License, (the "License").

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 ******************************************************************************/

#include "rpcgenerator.h"
#include "utilities.h"
#include "rpcplugin.h"

#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/compiler/plugin.pb.h>
#include <google/protobuf/compiler/cpp/cpp_generator.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <iostream>
#include <sstream>
#include <list>
#include <climits>

using namespace std;
using namespace google::protobuf;
using namespace google::protobuf::compiler;
using namespace google::protobuf::compiler::cpp;

namespace ProtoCall {
namespace Compiler {

RpcGenerator::RpcGenerator()
{
// TODO Auto-generated constructor stub

}

string RemoveExtern(string name)
{
  string ext("external.");
  size_t index = name.find(ext);
  if (index != string::npos)
    name.erase(0, ext.length());

  return name;
}

list<string> getExternalTypes(const google::protobuf::ServiceDescriptor *descriptor)
{
  string ext("external.");
  list<string> types;
  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);
    string input_type = method->input_type()->full_name();
    string output_type = method->output_type()->full_name();

    if (input_type.find(ext) == 0)
      types.push_back(input_type.erase(0,ext.length()));

    if (output_type.find(ext) == 0)
      types.push_back(output_type.erase(0,ext.length()));

  }

  return types;
}


void RpcGenerator::generateMethodSignature(const MethodDescriptor *method, io::Printer &printer) const
{
   string input_type = RemoveExtern(method->input_type()->full_name());
   string output_type = RemoveExtern(method->output_type()->full_name());

   printer.Print("$method_name$(", "method_name", method->name());

   if (input_type != "Void")
     printer.Print("const $input_type$* input", "input_type", input_type);

   if (input_type != "Void" && output_type != "Void")
     printer.Print(", ");

   if (output_type != "Void")
     printer.Print("$output_type$* output", "output_type", output_type);

   if (input_type != "Void" || output_type != "Void")
        printer.Print(", ");

   printer.Print("::google::protobuf::Closure* done)");
}

void RpcGenerator::generateServiceHeader(
    google::protobuf::compiler::GeneratorContext * generator_context,
    const google::protobuf::ServiceDescriptor *descriptor,
    const string &protoName) const
{
  string header(descriptor->name());
  header += ".pb.h";

  string headerGuard =  descriptor->name() + "_H_";
  transform(headerGuard.begin(), headerGuard.end(),headerGuard.begin(),
      ::toupper);

  io::ZeroCopyOutputStream *output =
      generator_context->Open(header.c_str());
  io::Printer printer(output, '$');

  // Generate abstract service interface ...
  printer.Print("// Generated by the RPC compiler.  DO NOT EDIT!\n\n");

  this->printHeaderGuardStart(printer, headerGuard);

  printer.Print("#include \"$protoName$.pb.h\"\n"
                "#include \"$protoName$_rpc.pb.h\"\n"
                "#include <protocall/runtime/rpcchannel.h>\n"
                "#include <protocall/runtime/service.h>\n",
                "protoName", protoName);

  list<string> types = getExternalTypes(descriptor);
  for (list<string>::iterator it = types.begin(); it != types.end(); it++)
    printer.Print("#include \"$class$.pb.h\"\n", "class", *it);

  printer.Print("\n");

  printer.Print("class $service$_Proxy;\n",
                "service", descriptor->full_name());

  printer.Print("class $service$_Dispatcher;\n\n",
                "service", descriptor->full_name());

  printer.Print("// Service definition\n"
    "class $service$: public ProtoCall::Runtime::Service {\n"
    "public:\n",
    "service", descriptor->full_name());

  printer.Indent();

  printer.Print("virtual ~$service$() {};\n","service",
      descriptor->full_name());

  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);

    printer.Print("virtual void ");
    this->generateMethodSignature(method, printer);
    printer.Print(" = 0;\n");
  }

  printer.Print("// Internal method");
  printer.Print("static list<string> messageTypes();\n");

  printer.Print("\n// typedef to make things look pretty for client\n"
                "typedef $service$_Proxy Proxy;\n",
                "service", descriptor->full_name());
  printer.Print("\n// typedef to make things look pretty for client\n"
                "typedef $service$_Dispatcher Dispatcher;\n",
                "service", descriptor->full_name());

  printer.Outdent();

  printer.Print("};");

  // Generate the proxy class
  printer.Print("\n\n// The proxy interface that is used by the client\n"
      "class $service$_Proxy : public $service$ {\n"
      "public:\n",
      "service", descriptor->full_name());

  printer.Indent();

  printer.Print("$service$_Proxy(::ProtoCall::Runtime::RpcChannel *channel);\n", "service",
      descriptor->full_name());

  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);
    printer.Print("void ");
    this->generateMethodSignature(method, printer);
    printer.Print(";\n");
  }

  printer.Outdent();

  //  Private
  printer.Print("\nprivate:\n");
  printer.Indent();
  printer.Print("::ProtoCall::Runtime::RpcChannel *m_channel;\n");

  printer.Outdent();
  printer.Print("};\n");

  this->printHeaderGuardEnd(printer, headerGuard);
}

void RpcGenerator::generateServiceProxyCc(google::protobuf::compiler::GeneratorContext * generator_context,
    const google::protobuf::ServiceDescriptor *descriptor) const
{
  string header(descriptor->name());
  header += ".pb.h";
  string cpp(descriptor->name());
  cpp += "_Proxy.pb.cc";

  io::ZeroCopyOutputStream *output = generator_context->Open(cpp.c_str());
  io::Printer cppPrinter(output, '$');

  cppPrinter.Print("#include \"$header$\"\n", "header", header.c_str());
  cppPrinter.Print("#include \"$class$_Handler.pb.h\"\n\n", "class", descriptor->name());
  cppPrinter.Print("#include <google/protobuf/stubs/common.h>\n");
  cppPrinter.Print("using google::protobuf::uint64;\n");

  // Add constructor implemention
  cppPrinter.Print("$service$_Proxy::$service$_Proxy(ProtoCall::Runtime::RpcChannel *channel)\n",
      "service", descriptor->full_name());

  cppPrinter.Print(" : m_channel(channel)\n{\n\n}\n\n");

  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);

    cppPrinter.Print("void $service$_Proxy::", "service", descriptor->full_name());
    this->generateMethodSignature(method, cppPrinter);
    cppPrinter.Print("\n{\n");

    cppPrinter.Indent();
    cppPrinter.Print("// Add RPC code here\n");
    string inputType = method->input_type()->name();
    string outputType = method->output_type()->name();
    cppPrinter.Print("rpc::Message msg;\n");
    cppPrinter.Print("rpc::Request *request = msg.mutable_request();\n");
    if (!isVoidType(inputType) || !isVoidType(outputType)) {
      cppPrinter.Print("uint64 id = this->m_channel->nextRequestId();\n");
      cppPrinter.Print("request->set_id(id);\n");
      cppPrinter.Print("$service$_Handler *handler = new $service$_Handler();\n",
          "service", descriptor->name());
      cppPrinter.Print("m_channel->registerResponseCallback(id, output, handler, done);\n");
    }
    cppPrinter.Print("$input_type$ *ext = request->MutableExtension($full_name$_request);\n",
        "input_type", inputType, "full_name", toExtensionName(method->full_name()));

    if (!isVoidType(inputType))
      cppPrinter.Print("ext->CopyFrom(*input);\n");

    // Send the message
    this->generateErrorCheck("m_channel->send(&msg)", cppPrinter);

    // Now check if we have any vtk objects to send
    this->generateSentVtkBlock("input", "m_channel",
        method->input_type(), cppPrinter);

    // Now check if we have any external objects to send
    this->generateSentExternalBlock("input", "m_channel",
        method->input_type(), cppPrinter);

    cppPrinter.Outdent();

    cppPrinter.Print("}\n\n");
  }

  //this->GenerateCallMethod(descriptor, cppPrinter);
}


bool RpcGenerator::Generate(const FileDescriptor * file,
                         const string & parameter,
                         GeneratorContext * generator_context,
                         string * error) const
{
  string protoName = file->name();
  protoName = protoName.substr(0, protoName.find(".proto"));

  for(int i=0; i<file->service_count(); i++  ) {
    const google::protobuf::ServiceDescriptor *descriptor = file->service(i);
    this->generateServiceHeader(generator_context, descriptor, protoName);
    this->generateServiceProxyCc(generator_context, descriptor);
    this->generateDispatcherHeader(generator_context, descriptor, protoName);
    this->generateDispatcherCc(generator_context, descriptor);
    this->generateResponseHandler(generator_context, descriptor);
  }

  return true;
}

void RpcGenerator::generateCallMethod(const google::protobuf::ServiceDescriptor *descriptor,
                                   google::protobuf::io::Printer &printer) const
{
  printer.Print("void $service$::callMethod(const string &methodName, const Message* request, Message* response, Closure* done)\n",
                "service", descriptor->full_name());
  printer.Print("{\n");
  printer.Print("\n");
  printer.Print("}\n");
}


void RpcGenerator::printHeaderGuardStart(google::protobuf::io::Printer &printer,
    const string &define) const
{
  printer.Print("#ifndef $define$\n"
                "#define $define$\n\n", "define", define);
}

void RpcGenerator::printHeaderGuardEnd(google::protobuf::io::Printer &printer,
    const string &define) const
{
  printer.Print("#endif /* $define$ */\n", "define", define);
}

static const string dispatchSignature
  = "dispatch(int methodId, rpc::Message *requestMessageEnvelope,"
      "const google::protobuf::Message *request, ProtoCall::Runtime::RpcChannel *replyChannel)";

static const string replySignature
  = "reply(ProtoCall::Runtime::ReplyInfo info)";

static const string implementsSignature
  = "implements()";

static const string implementsReturnType
  = "std::vector<int>";

void RpcGenerator::generateDispatcherHeader(google::protobuf::compiler::GeneratorContext *generator_context,
                           const google::protobuf::ServiceDescriptor *descriptor,
                           const string &protoFileName) const
{
  string name = descriptor->name() + "_Dispatcher";
  string header = name + ".pb.h";;

  string headerGuard =  name + "_H_";
  transform(headerGuard.begin(), headerGuard.end(),headerGuard.begin(),
      ::toupper);

  io::ZeroCopyOutputStream *output =
      generator_context->Open(header.c_str());
  io::Printer printer(output, '$');

  // Generate abstract service interface ...
  printer.Print("// Generated by the RPC compiler.  DO NOT EDIT!\n\n");

  this->printHeaderGuardStart(printer, headerGuard);

  string service = descriptor->name();

  printer.Print("#include \"$service$.pb.h\"\n", "service", service);
  printer.Print("#include <protocall/runtime/servicedispatcher.h>\n");

  printer.Print("// Dispatcher definition\n"
    "class $service$_Dispatcher: public ProtoCall::Runtime::ServiceDispatcher {\n"
    "public:\n",
    "service", descriptor->full_name());

  printer.Indent();

  printer.Print("$service$_Dispatcher($service$ *service);\n",
      "service", service);

  printer.Print("void $dispatchSignature$;\n", "dispatchSignature",
      dispatchSignature);

  printer.Print("void $replySignature$;\n", "replySignature",
      replySignature);

  printer.Print("$implementsReturnType$ $implementsSignature$;\n",
      "implementsReturnType", implementsReturnType,
      "implementsSignature",  implementsSignature);

  printer.Outdent();

  printer.Print("};\n");

  this->printHeaderGuardEnd(printer, headerGuard);
}

void RpcGenerator::generateDispatchMethodBody(
    const google::protobuf::ServiceDescriptor *descriptor,
    google::protobuf::io::Printer &printer) const
{
  printer.Indent();

  printer.Print("switch(methodId) { \n");
  printer.Indent();

  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);
    string methodName = method->full_name();
    int methodId = RpcPlugin::generateExtensionNumber(methodName);
    string inputTypeName = method->input_type()->name();
    string outputTypeName = method->output_type()->name();
    ostringstream methodIdStr;
    methodIdStr << methodId;
    printer.Print("case $methodId$ : {\n", "methodId", methodIdStr.str());
    printer.Indent();
    printer.Print("// $methodName$\n", "methodName",  methodName);

    if (!isVoidType(inputTypeName)) {
      printer.Print("const $inputType$ *in = static_cast<const $inputType$ *>(request);\n",
          "inputType", inputTypeName);

      // Receive any VTK types that might be on the wire ...
      generateReceiveVtkBlock("request", "replyChannel",
          method->input_type(),printer);

      // Receive any external type that might be on the wire ...
      generateReceiveExternalBlock("request", "replyChannel",
          method->input_type(),printer);
    }

    if (!isVoidType(outputTypeName)) {
      printer.Print("$outputType$ *out = new $outputType$();\n",
          "outputType", outputTypeName);
      printer.Print("ProtoCall::Runtime::ReplyInfo info;\n");
      printer.Print("info.methodId = $methodId$;\n", "methodId", methodIdStr.str());
      printer.Print("info.requestMessageEnvelope = requestMessageEnvelope;\n");
      printer.Print("info.response = out;\n");
      printer.Print("info.replyChannel = replyChannel;\n");
      string dispatcherName = descriptor->name() + "_Dispatcher";
      printer.Print("::google::protobuf::Closure *done = "
          "::google::protobuf::NewCallback(this, &$dispatcherName$::reply, info);\n",
          "dispatcherName", dispatcherName);
    }
    else {
      printer.Print("::google::protobuf::Closure *done = google::protobuf::NewCallback(&google::protobuf::DoNothing);\n");
    }

    printer.Print("static_cast<$service$ *>(m_service)->$methodName$(", "service",
        descriptor->name(), "methodName", method->name());

    if (!isVoidType(inputTypeName))
      printer.Print("in");

    if (!isVoidType(inputTypeName) && !isVoidType(outputTypeName))
      printer.Print(", ");

    if (!isVoidType(outputTypeName))
      printer.Print("out");

    if (!isVoidType(inputTypeName) || !isVoidType(outputTypeName))
      printer.Print(", ");

    printer.Print("done);\n");


    printer.Outdent();

    printer.Print("break;\n");
    printer.Print("}\n");
  }

  printer.Print("default:\n");
  printer.Indent();
  printer.Print("replyChannel->setErrorString(\"No case for messageId: \" +  methodId);");
  printer.Outdent();
  printer.Outdent();
  printer.Print("}\n");


  printer.Outdent();

}

void RpcGenerator::generateDispatcherCc(google::protobuf::compiler::GeneratorContext *generator_context,
                        const google::protobuf::ServiceDescriptor *descriptor) const
{
  string header =  descriptor->name() + "_Dispatcher.pb.h";
  string cpp = descriptor->name() + "_Dispatcher.pb.cc";

  io::ZeroCopyOutputStream *output = generator_context->Open(cpp.c_str());
  io::Printer printer(output, '$');

  printer.Print("#include \"");
  printer.Print(header.c_str());
  printer.Print("\"\n\n");
  printer.Print("#include <iostream>\n");
  printer.Print("#include <memory>\n");
  printer.Print("#include <google/protobuf/stubs/common.h>\n");
  printer.Print("using google::protobuf::uint64;\n");

  // Add include for external serializers
  set<string> externalTypes = this->extractExternalTypes(descriptor);

  for(set<string>::const_iterator it = externalTypes.begin();
      it != externalTypes.end(); ++it)
  {
    string path = externalTypeToHeaderPath(*it);
    string cls = extractClassName(*it);
    transform(cls.begin(), cls.end(), cls.begin(),
      ::tolower);

    printer.Print("#include <$path$$cls$serializer.h>\n", "path", path,
        "cls", cls);
  }

  // Add constructor implemention
  printer.Print("$service$_Dispatcher::$service$_Dispatcher($service$ *service)\n",
     "service", descriptor->full_name());

  printer.Print(" : ProtoCall::Runtime::ServiceDispatcher(service)\n{\n\n}\n\n");

  printer.Print("void $service$_Dispatcher::$dispatchSignature$\n{\n",
      "service", descriptor->full_name(), "dispatchSignature",
      dispatchSignature);

  this->generateDispatchMethodBody(descriptor, printer);

  printer.Print("}\n");

  this->generateRelyMethod(descriptor, printer);
  this->generateImplementsMethod(descriptor, printer);
}



void RpcGenerator::generateRelyMethod(
    const google::protobuf::ServiceDescriptor *descriptor,
    google::protobuf::io::Printer &printer) const
{
  printer.Print("void $service$_Dispatcher::$replySignature$\n{\n",
      "service", descriptor->full_name(), "replySignature",
      replySignature);

  printer.Indent();

  printer.Print("switch(info.methodId) {\n");
  printer.Indent();

  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);
    string methodName = method->full_name();
    int methodId = RpcPlugin::generateExtensionNumber(methodName);
    string inputTypeName = method->input_type()->name();
    string outputTypeName = method->output_type()->name();

    if (isVoidType(outputTypeName))
      continue;

    ostringstream methodIdStr;
    methodIdStr << methodId;
    printer.Print("case $methodId$ : {\n", "methodId", methodIdStr.str());
    printer.Indent();
    printer.Print("// $methodName$\n", "methodName",  methodName);

    printer.Print("// Prepare the response\n");
    printer.Print("rpc::Message msg;\n");
    printer.Print("rpc::Response *response = msg.mutable_response();\n");
    printer.Print("uint64 requestId = info.requestMessageEnvelope->request().id();\n");
    printer.Print("response->set_id(requestId);\n");
    string extensionName = toExtensionName(method->full_name());
    std::ostringstream var;
    var << "tmp" << j;

    printer.Print("$responseType$ *$var$ = static_cast<$responseType$ *>(info.response);\n",
        "responseType", outputTypeName, "var", var.str());

    // Add error info
    printer.Print("if ($var$->hasError()) {\n", "var", var.str());
    printer.Indent();
    printer.Print("response->set_errorstring($var$->errorString());\n", "var", var.str());
    printer.Print("response->set_errorcode($var$->errorCode());\n", "var", var.str());
    printer.Outdent();
    printer.Print("}\n");
    printer.Print("response->SetAllocatedExtension($extensionName$, $var$);\n",
        "extensionName", extensionName + "_response", "var", var.str());
    printer.Print("// Make sure request is cleaned up\n");
    printer.Print("std::auto_ptr<rpc::Message> request;\n");
    printer.Print("request.reset(info.requestMessageEnvelope);\n");
    this->generateErrorCheck("info.replyChannel->send(&msg)", printer);

    // send any VTK object we need to.
    this->generateSentVtkBlock(var.str(), "info.replyChannel",
        method->output_type(), printer);

    // send any external objects we need to.
    this->generateSentExternalBlock(var.str(), "info.replyChannel",
        method->output_type(), printer);

    printer.Outdent();
    printer.Print("break;\n");
    printer.Print("}\n");
  }

  printer.Print("default:\n");
  printer.Indent();
  printer.Print("// TODO error case\n");
  printer.Print("info.replyChannel->setErrorString(\"No reply case for messageId: \" + info.methodId);\n");
  printer.Outdent();
  printer.Outdent();
  printer.Print("}\n");
  printer.Outdent();
  printer.Print("}\n");
}

void RpcGenerator::generateImplementsMethod(const google::protobuf::ServiceDescriptor *descriptor,
    google::protobuf::io::Printer &printer) const
{
  printer.PrintRaw(implementsReturnType);


  printer.Print(" $serviceName$_Dispatcher::$implementsSignature$\n{\n",
      "serviceName", descriptor->name(),
      "implementsSignature",  implementsSignature);
  printer.Indent();
  printer.Print("// This is NOT thread safe ...\n");
  printer.Print("static $implementsReturnType$ methodIds;\n",
      "implementsReturnType", implementsReturnType);
  printer.Print("static bool init = true;\n");
  printer.Print("if (init) {\n");
  printer.Indent();
  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);
    string methodName = method->full_name();
    int methodId = RpcPlugin::generateExtensionNumber(methodName);
    ostringstream methodIdStr;
    methodIdStr << methodId;

    printer.Print("methodIds.push_back($methodId$);\n", "methodId",
        methodIdStr.str());
  }
  printer.Print("init = false;\n");
  printer.Outdent();
  printer.Print("}\n");
  printer.Print("return methodIds;\n");
  printer.Outdent();
  printer.Print("}\n");
}

void RpcGenerator::generateSentVtkBlock(const string &variableName,
    const string &channelName, const google::protobuf::Descriptor *descriptor,
    google::protobuf::io::Printer &printer) const
{
  for(int i=0; i< descriptor->field_count(); i++) {
    const FieldDescriptor *field = descriptor->field(i);

    if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
      string messageTypeName = field->message_type()->full_name();

      if (messageTypeName.find("vtk.") == 0) {
        string vtkType = messageTypeName.substr(4);

        std::ostringstream stream;
        stream << "obj" << i;
        string var = stream.str();
        printer.Print("if ($variableName$->has_$fieldName$()) {\n",
          "variableName", variableName, "fieldName", field->lowercase_name());
        printer.Indent();
        printer.Print("$vtkType$ *$var$ = ", "vtkType", vtkType, "var", var);
        printer.Print("$variableName$->$fieldName$().get();\n", "variableName",
            variableName, "fieldName", field->lowercase_name());

        printer.Print("if ($var$) {\n", "var", var);
        printer.Indent();

        std::map<string, string> variables;
        variables["var"] = var;
        variables["channelName"] = channelName;

        this->generateErrorCheck("$channelName$->send($var$)", variables,
            printer);
        printer.Outdent();
        printer.Print("}\n");
        printer.Outdent();
        printer.Print("}\n");
      }
    }
  }
}

void RpcGenerator::generateReceiveVtkBlock(const string &variableName,
    const string &channelName, const google::protobuf::Descriptor *descriptor,
    google::protobuf::io::Printer &printer) const
{
  bool hasVtkTypes = false;

  string code;
  io::StringOutputStream stream(&code);
  io::Printer tmpPrinter(&stream, '$');

  for(int i=0; i< descriptor->field_count(); i++) {
    const FieldDescriptor *field = descriptor->field(i);

    if (field->type() == FieldDescriptor::TYPE_MESSAGE)
    {
      string messageTypeName = field->message_type()->full_name();

      if (messageTypeName.find("vtk.") == 0) {
        hasVtkTypes = true;

        string vtkType = messageTypeName.substr(4);

        std::ostringstream stream;
        stream << "obj" << i;
        string var = stream.str();

        tmpPrinter.Print("if ($variableName$->has_$fieldName$()) {\n",
          "variableName", "tmp", "fieldName", field->lowercase_name());
        tmpPrinter.Indent();
        tmpPrinter.Print("$vtkType$ *$var$ = tmp->$fieldName$().get();\n",
            "var", var, "vtkType", vtkType, "fieldName", field->lowercase_name());
        tmpPrinter.Print("bool allocated = false;\n");
        tmpPrinter.Print("if (!$var$) {\n", "var", var);
        tmpPrinter.Indent();
        tmpPrinter.Print("$var$ = $vtkType$::New();\n",
            "var", var, "vtkType", vtkType);
        tmpPrinter.Print("allocated = true;\n");
        tmpPrinter.Outdent();
        tmpPrinter.Print("}");

        std::map<string, string> variables;
        variables["var"] = var;
        variables["channelName"] = channelName;
        this->generateErrorCheck("$channelName$->receive($var$)", variables,
            tmpPrinter, "$var$->Delete();return;\n");

        tmpPrinter.Print("if (allocated)\n");
        tmpPrinter.Indent();
        tmpPrinter.Print("tmp->mutable_$field$()->set_allocated($var$);\n",
            "field", field->lowercase_name(), "var", var);
        tmpPrinter.Outdent();
        tmpPrinter.Outdent();
        tmpPrinter.Print("}\n");
      }
    }
  }

  if (hasVtkTypes) {
    printer.Print("::google::protobuf::Message *mut "
        "= const_cast< google::protobuf::Message *>($variableName$);\n",
        "variableName", variableName);
    printer.Print("$inputType$ *tmp ="
        "static_cast<$inputType$ *>(mut);\n",
        "inputType", descriptor->name());
    printer.Print(code.c_str());
  }
}

const static string handleSignature = "handle(ProtoCall::Runtime::RpcChannel *channel, "
    "int methodId, google::protobuf::Message *incomingResponse,"
    "google::protobuf::Message *targetResponse,"
    "google::protobuf::int32 errorCode,"
    "std::string errorString,"
    "google::protobuf::Closure *callback)";

void RpcGenerator::generateResponseHandler(google::protobuf::compiler::GeneratorContext *generator_context,
    const google::protobuf::ServiceDescriptor *descriptor) const
{
  this->generateResponseHandlerHeader(generator_context, descriptor);
  this->generateResponseHandlerCpp(generator_context, descriptor);
}

void RpcGenerator::generateResponseHandlerHeader(google::protobuf::compiler::GeneratorContext *generator_context,
    const google::protobuf::ServiceDescriptor *descriptor) const
{
  string cls = descriptor->name();
  string header = cls + "_Handler.pb.h";

  string headerGuard =  cls + "HANDLER_H_";
  transform(headerGuard.begin(), headerGuard.end(),headerGuard.begin(),
      ::toupper);

  io::ZeroCopyOutputStream *output =
      generator_context->Open(header.c_str());
  io::Printer printer(output, '$');

  // Generate abstract service interface ...
  printer.Print("// Generated by the RPC compiler.  DO NOT EDIT!\n\n");

  this->printHeaderGuardStart(printer, headerGuard);

  printer.Print("#include \"$class$.pb.h\"\n", "class",
      cls);
  printer.PrintRaw("#include <protocall/runtime/responsehandler.h>\n");
  printer.Print("// Handler definition\n"
    "class $class$_Handler: public ProtoCall::Runtime::ResponseHandler {\n"
    "public:\n",
    "class", cls);

  printer.Indent();
  // Constructor
  printer.Print("$class$_Handler();\n", "class", cls);
  printer.Print("void $signature$;\n", "signature", handleSignature);
  printer.Outdent();
  printer.Print("};\n");
  this->printHeaderGuardEnd(printer, headerGuard);
}

void RpcGenerator::generateResponseHandlerCpp(google::protobuf::compiler::GeneratorContext *generator_context,
    const google::protobuf::ServiceDescriptor *descriptor) const
{
  string cls = descriptor->name() + "_Handler";
  string cpp = cls + ".pb.cc";

  io::ZeroCopyOutputStream *output =
      generator_context->Open(cpp.c_str());
  io::Printer printer(output, '$');

  printer.Print("// Generated by the RPC compiler.  DO NOT EDIT!\n\n");

  printer.Print("#include \"$class$.pb.h\"\n\n", "class", cls);

  // Add includes for external deserializers
  set<string> externalTypes = this->extractExternalTypes(descriptor);

  for(set<string>::const_iterator it = externalTypes.begin();
      it != externalTypes.end(); ++it)
  {
    string path = externalTypeToHeaderPath(*it);
    string cls = extractClassName(*it);
    transform(cls.begin(), cls.end(), cls.begin(),
      ::tolower);

    printer.Print("#include <$path$$cls$deserializer.h>\n", "path", path,
        "cls", cls);
  }

  // Constructor
  printer.Print("$class$::$class$() \n{\n}\n", "class", cls);

  printer.Print("void $class$::$signature$ {\n", "class", cls, "signature",
      handleSignature);

  printer.Indent();
  printer.Print("switch(methodId) {\n");
  printer.Indent();

  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);

    string methodName = method->full_name();
    int methodId = RpcPlugin::generateExtensionNumber(methodName);
    string inputTypeName = method->input_type()->name();
    string outputTypeName = method->output_type()->name();

    if (isVoidType(outputTypeName))
      continue;

    ostringstream methodIdStr;
    methodIdStr << methodId;
    printer.Print("case $methodId$ : {\n", "methodId", methodIdStr.str());
    printer.Indent();
    printer.Print("// $methodName$\n", "methodName",  methodName);

    printer.Print("targetResponse->CopyFrom(const_cast<const google::protobuf::Message&>(*incomingResponse));\n");

    printer.Print("::google::protobuf::Message *mutableResponse "
        "= const_cast< google::protobuf::Message *>(targetResponse);\n");
    printer.Print("$responseType$ *responseType ="
        "static_cast<$responseType$ *>(mutableResponse);\n",
        "responseType", outputTypeName);
    printer.Print("responseType->setErrorCode(errorCode);\n");
    printer.Print("responseType->setErrorString(errorString);\n");


    const Descriptor *responseType = method->output_type();
    this->generateReceiveVtkBlock("targetResponse", "channel", responseType,
        printer);

    // Receive any external objects
    this->generateReceiveExternalBlock("targetResponse", "channel", responseType,
            printer);

    // Now call the users callback
    printer.Print("callback->Run();\n");

    printer.Outdent();
    printer.Print("break;\n");
    printer.Print("}\n");
  }
  printer.Outdent();
  printer.Print("}\n");
  printer.Outdent();
  printer.Print("}\n");
}

void RpcGenerator::generateErrorCheck(const char *call,
    google::protobuf::io::Printer &printer, const char *blockText) const
{
  std::map< string, string > variables;
  this->generateErrorCheck(call, variables, printer, blockText);
}

void RpcGenerator::generateErrorCheck(const char *call,
    std::map< string, string > &variables,
    google::protobuf::io::Printer &printer,  const char *blockText) const
{
  string code;
  io::StringOutputStream stream(&code);
  io::Printer tmpPrinter(&stream, '$');
  tmpPrinter.Print("if (!$call$) {\n", "call", call);
  tmpPrinter.Indent();

  if (blockText)
    tmpPrinter.Print("$blockText$", "blockText", blockText);
  else
    tmpPrinter.Print("return;\n");

  tmpPrinter.Outdent();
  tmpPrinter.Print("}\n");

  printer.Print(variables, code.c_str());
}

void RpcGenerator::generateSentExternalBlock(const string &variableName,
    const string &channelName, const google::protobuf::Descriptor *descriptor,
    google::protobuf::io::Printer &printer) const
{
  for(int i=0; i< descriptor->field_count(); i++) {
    const FieldDescriptor *field = descriptor->field(i);

    if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
      string messageTypeName = field->message_type()->full_name();

      if (messageTypeName.find("external.") == 0) {
        string externalClass = externalTypeToClass(messageTypeName);

        std::ostringstream stream;
        stream << "obj" << i;
        string var = stream.str();
        printer.Print("if ($variableName$->has_$fieldName$()) {\n",
          "variableName", variableName, "fieldName", field->lowercase_name());
        printer.Indent();
        printer.Print("$externalClass$ *$var$ = ", "externalClass", externalClass, "var", var);
        printer.Print("$variableName$->$fieldName$().get();\n", "variableName",
            variableName, "fieldName", field->lowercase_name());

        printer.Print("if ($var$) {\n", "var", var);
        printer.Indent();

        printer.Print("$externalClass$Serializer serializer($var$);\n",
            "externalClass", externalClass, "var", var);
        printer.Print("size_t size = serializer.size();\n");
        std::map<string, string> variables;
        variables["channelName"] = channelName;
        printer.Print("std::vector<unsigned char> data(size);\n");
        this->generateErrorCheck("serializer.serialize(&data[0], size)", variables,
            printer, "$channelName$->setErrorString(\"Serialization failed\");return;\n");

        variables.clear();
        variables["channelName"] = channelName;

        // Send the size first
        this->generateErrorCheck("$channelName$->send(size)", variables,
                    printer);

        this->generateErrorCheck("$channelName$->send(&data[0], size)", variables,
            printer);
        printer.Outdent();
        printer.Print("}\n");
        printer.Outdent();
        printer.Print("}\n");
      }
    }
  }
}

void RpcGenerator::extractExternalTypes(
    const google::protobuf::Descriptor *descriptor,
    set<string> &externalTypes) const
{
  for(int i=0; i< descriptor->field_count(); i++) {
    const FieldDescriptor *field = descriptor->field(i);

    if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
      string messageTypeName = field->message_type()->full_name();

      if (messageTypeName.find("external.") == 0)
        externalTypes.insert(messageTypeName);

    }
  }
}

set<string> RpcGenerator::extractExternalTypes(
    const google::protobuf::ServiceDescriptor *descriptor) const
{
  set<string> externalTypes;

  for(int j=0; j<descriptor->method_count(); j++) {
    const MethodDescriptor *method = descriptor->method(j);

    const Descriptor *inputDescriptor = method->input_type();
    this->extractExternalTypes(inputDescriptor, externalTypes);

    const Descriptor *outputDescriptor = method->output_type();
    this->extractExternalTypes(outputDescriptor, externalTypes);

  }

  return externalTypes;
}

void RpcGenerator::generateReceiveExternalBlock(const string &variableName,
    const string &channelName, const google::protobuf::Descriptor *descriptor,
    google::protobuf::io::Printer &printer) const
{
  bool hasExternalTypes = false;

  string code;
  io::StringOutputStream stream(&code);
  io::Printer tmpPrinter(&stream, '$');

  for(int i=0; i< descriptor->field_count(); i++) {
    const FieldDescriptor *field = descriptor->field(i);

    if (field->type() == FieldDescriptor::TYPE_MESSAGE)
    {
      string messageTypeName = field->message_type()->full_name();

      if (messageTypeName.find("external.") == 0) {
        hasExternalTypes = true;

        string externalClass = externalTypeToClass(messageTypeName);

        std::ostringstream objStream;
        objStream << "obj" << i;
        string objVar = objStream.str();

        std::ostringstream sizeStream;
        sizeStream << "size" << i;
        string sizeVar = sizeStream.str();

        std::ostringstream deserializerStream;
        deserializerStream << "deserializer" << i;
        string deserializerVar = deserializerStream.str();

        std::ostringstream dataStream;
        dataStream << "data" << i;
        string dataVar = dataStream.str();

        tmpPrinter.Print("if ($variableName$->has_$fieldName$()) {\n",
          "variableName", "tmp", "fieldName", field->lowercase_name());
        tmpPrinter.Indent();

        tmpPrinter.Print("unsigned int $sizeVar$;\n", "sizeVar", sizeVar);
        std::map<string, string> variables;
        variables["var"] = sizeVar;
        variables["channelName"] = channelName;
        this->generateErrorCheck("$channelName$->receive(&$var$)", variables,
                    tmpPrinter);
        tmpPrinter.Print("std::vector<unsigned char> $data$($size$);\n", "data",
            dataVar, "size", sizeVar);
        variables["var"] = dataVar;
        variables["size"] = sizeVar;
        this->generateErrorCheck("$channelName$->receive(&$var$[0], $size$)", variables,
                    tmpPrinter);

        tmpPrinter.Print("$externalClass$ *$objVar$ = tmp->$field$().get();\n",
            "externalClass", externalClass, "objVar", objVar, "field", field->lowercase_name());
        tmpPrinter.Print("bool allocated = false;\n");
        tmpPrinter.Print("if (!$objVar$) {\n", "objVar", objVar);
        tmpPrinter.Indent();
        tmpPrinter.Print("$objVar$ = new $externalClass$();\n",
            "objVar", objVar, "externalClass", externalClass);
        tmpPrinter.Print("allocated = true;\n");
        tmpPrinter.Outdent();
        tmpPrinter.Print("}\n");

        tmpPrinter.Print("$externalClass$Deserializer $deserializerVar$($objVar$);\n",
            "externalClass", externalClass, "deserializerVar", deserializerVar,
            "objVar", objVar);
        variables["deserializerVar"] = deserializerVar;
        this->generateErrorCheck("$deserializerVar$.deserialize(&$var$[0], $size$)", variables,
            tmpPrinter, "$channelName$->setErrorString(\"Deserialization failed\");return;\n");

        variables.clear();
        variables["var"] = objVar;
        variables["channelName"] = channelName;
        variables["objVar"] = objVar;

        tmpPrinter.Print("if (allocated)\n");
        tmpPrinter.Indent();
        tmpPrinter.Print("tmp->mutable_$field$()->set_allocated($objVar$);\n",
            "field", field->lowercase_name(), "objVar", objVar);
        tmpPrinter.Outdent();
        tmpPrinter.Outdent();
        tmpPrinter.Print("}\n");
      }
    }
  }

  if (hasExternalTypes) {
    printer.Print("::google::protobuf::Message *mutableRequest "
        "= const_cast< google::protobuf::Message *>($variableName$);\n",
        "variableName", variableName);
    printer.Print("$inputType$ *tmp ="
        "static_cast<$inputType$ *>(mutableRequest);\n",
        "inputType", descriptor->name());
    printer.Print(code.c_str());
  }
}

}
}
